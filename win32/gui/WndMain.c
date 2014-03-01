/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2003  Pcsx Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "resource.h"
#include "AboutDlg.h"

#include "psxcommon.h"
#include "plugin.h"
#include "debug.h"
#include "Win32.h"
#include "sio.h"
#include "misc.h"
#include "cheat.h"

#ifdef __MINGW32__
#ifndef LVM_GETSELECTIONMARK
#define LVM_GETSELECTIONMARK (LVM_FIRST+66)
#endif
#ifndef ListView_GetSelectionMark
#define ListView_GetSelectionMark(w) (INT)SNDMSG((w),LVM_GETSELECTIONMARK,0,0)
#endif
#endif

int AccBreak = 0;
int ConfPlug = 0;
int StatesC = 0;
int CancelQuit = 0;
char cfgfile[256];
int Running = 0;
boolean UseGui = TRUE;
char PcsxDir[256];

static HDC          hDC;
static HDC          hdcmem;
static HBITMAP      hBm;
static BITMAP       bm;

#ifdef ENABLE_NLS

unsigned int langsMax;

typedef struct {
	char lang[256];
} _langs;
_langs *langs = NULL;

typedef struct {
	char			id[8];
	char			name[64];
	LANGID			langid;
} LangDef;

LangDef sLangs[] = {
	{ "ar",		N_("Arabic"),				0x0401 },
	{ "ca",		N_("Catalan"),				0x0403 },
	{ "de",		N_("German"),				0x0407 },
	{ "el",		N_("Greek"),				0x0408 },
	{ "en",		N_("English"),				0x0409 },
	{ "es",		N_("Spanish"),				0x040a },
	{ "fr_FR",	N_("French"),				0x040c },
	{ "it",		N_("Italian"),				0x0410 },
	{ "pt",		N_("Portuguese"),			0x0816 },
	{ "pt_BR",	N_("Portuguese (Brazilian)"),		0x0416 },
	{ "ro",		N_("Romanian"),				0x0418 },
	{ "ru_RU",	N_("Russian"),				0x0419 },
	{ "zh_CN",	N_("Simplified Chinese"),	0x0804 },
	{ "zh_TW",	N_("Traditional Chinese"),	0x0404 },
	{ "ja",		N_("Japanese"),				0x0411 },
	{ "ko",		N_("Korean"),				0x0412 },
	{ "", "", 0xFFFF },
};

char *ParseLang(char *id) {
	int i=0;

	while (sLangs[i].id[0] != '\0') {
		if (!strcmp(id, sLangs[i].id))
			return _(sLangs[i].name);
		i++;
	}

	return id;
}

static void SetDefaultLang(void) {
	LANGID langid;
	int i;

	langid = GetSystemDefaultLangID();

	i = 0;
	while (sLangs[i].id[0] != '\0') {
		if (langid == sLangs[i].langid) {
			strcpy(Config.Lang, sLangs[i].id);
			return;
		}
		i++;
	}

	strcpy(Config.Lang, "English");
}

#endif

void strcatz(char *dst, char *src) {
	int len = strlen(dst) + 1;
	strcpy(dst + len, src);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	char *arg = NULL;
	char cdfile[MAXPATHLEN] = "", buf[4096];
	int loadstatenum = -1;

	strcpy(cfgfile, "Software\\Pcsx");

	gApp.hInstance = hInstance;

#ifdef ENABLE_NLS
	bindtextdomain(PACKAGE, "Langs\\");
	textdomain(PACKAGE);
#endif

	Running = 0;

	GetCurrentDirectory(256, PcsxDir);

	memset(&Config, 0, sizeof(PcsxConfig));
	strcpy(Config.Net, "Disabled");
	if (LoadConfig() == -1) {
		Config.PsxAuto = 1;
		strcpy(Config.PluginsDir, "Plugins\\");
		strcpy(Config.BiosDir,    "Bios\\");

		strcpy(Config.Mcd1, "memcards\\Mcd001.mcr");
		strcpy(Config.Mcd2, "memcards\\Mcd002.mcr");

		ConfPlug = 1;

#ifdef ENABLE_NLS
		{
			char text[256];
			SetDefaultLang();
			sprintf(text, "LANGUAGE=%s", Config.Lang);
			gettext_putenv(text);
		}
#endif

		ConfigurePlugins(gApp.hWnd);

		if (LoadConfig() == -1) {
			return 0;
		}
	}

	strcpy(Config.PatchesDir, "Patches\\");

#ifdef ENABLE_NLS
	if (Config.Lang[0] == 0) {
		SetDefaultLang();
		SaveConfig();
		LoadConfig();
	}
#endif

	// Parse command-line
	strncpy(buf, lpCmdLine, 4096);

	for (arg = strtok(buf, " "); arg != NULL; arg = strtok(NULL, " ")) {
		if (strcmp(arg, "-nogui") == 0) {
			UseGui = FALSE;
		} else if (strcmp(arg, "-runcd") == 0) {
			cdfile[0] = '\0';
		} else if (strcmp(arg, "-cdfile") == 0) {
			arg = strtok(NULL, " ");
			if (arg != NULL) {
				if (arg[0] == '"') {
					strncpy(buf, lpCmdLine + (arg - buf), 4096);
					arg = strtok(buf, "\"");
					if (arg != NULL) strcpy(cdfile, arg);
				} else {
					strcpy(cdfile, arg);
				}
				UseGui = FALSE;
			}
		} else if (strcmp(arg, "-psxout") == 0) {
			Config.PsxOut = TRUE;
		} else if (strcmp(arg, "-slowboot") == 0) {
			Config.SlowBoot = TRUE;
		} else if (strcmp(arg, "-help") == 0) {
			MessageBox(gApp.hWnd, _(
				"Usage: pcsx [options]\n"
				"\toptions:\n"
				"\t-nogui\t\tDon't open the GUI\n"
				"\t-psxout\t\tEnable PSX output\n"
				"\t-runcd\t\tRuns CD-ROM (requires -nogui)\n"
				"\t-cdfile FILE\tRuns a CD image file (requires -nogui)\n"
				"\t-help\t\tDisplay this message"),
				"PCSX", 0);

			return 0;
		}
	}

	if (SysInit() == -1) return 1;

	CreateMainWindow(nCmdShow);

	if (!UseGui) {
		SetIsoFile(cdfile);
		PostMessage(gApp.hWnd, WM_COMMAND, ID_FILE_RUN_NOGUI, 0);
	}

	RunGui();

	return 0;
}

void RunGui() {
	MSG msg;

	for (;;) {
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

void RestoreWindow() {
	AccBreak = 1;
	DestroyWindow(gApp.hWnd);
	CreateMainWindow(SW_SHOWNORMAL);
	ShowCursor(TRUE);
	SetCursor(LoadCursor(gApp.hInstance, IDC_ARROW));
	ShowCursor(TRUE);

	if (!UseGui) PostMessage(gApp.hWnd, WM_COMMAND, ID_FILE_EXIT, 0);
}

void ResetMenuSlots() {
	char str[256];
	int i;

	for (i = 0; i < 9; i++) {
		GetStateFilename(str, i);
		if (CheckState(str) == -1)
			EnableMenuItem(gApp.hMenu, ID_FILE_STATES_LOAD_SLOT1+i, MF_GRAYED);
		else 
			EnableMenuItem(gApp.hMenu, ID_FILE_STATES_LOAD_SLOT1+i, MF_ENABLED);
	}
}

void OpenConsole() {
	if (hConsole) return;
	AllocConsole();
	SetConsoleTitle("Psx Output");
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
}

void CloseConsole() {
	FreeConsole();
	hConsole = NULL;
}

void States_Load(int num) {
	char Text[256];
	int ret;

	SetMenu(gApp.hWnd, NULL);
	OpenPlugins(gApp.hWnd);

	GetStateFilename(Text, num);

	ret = LoadState(Text);
	if (ret == 0)
		 sprintf(Text, _("*PCSX*: Loaded State %d"), num+1);
	else sprintf(Text, _("*PCSX*: Error Loading State %d"), num+1);
	GPU_displayText(Text);

	Running = 1;
	CheatSearchBackupMemory();
	psxCpu->Execute();
}

void States_Save(int num) {
	char Text[256];
	int ret;

	SetMenu(gApp.hWnd, NULL);
	OpenPlugins(gApp.hWnd);

	GPU_updateLace();

	GetStateFilename(Text, num);
	GPU_freeze(2, (GPUFreeze_t *)&num);
	ret = SaveState(Text);
	if (ret == 0)
		 sprintf(Text, _("*PCSX*: Saved State %d"), num+1);
	else sprintf(Text, _("*PCSX*: Error Saving State %d"), num+1);
	GPU_displayText(Text);

	Running = 1;
	CheatSearchBackupMemory();
	psxCpu->Execute();
}

void OnStates_LoadOther() {
	OPENFILENAME ofn;
	char szFileName[MAXPATHLEN];
	char szFileTitle[MAXPATHLEN];
	char szFilter[256];

	memset(&szFileName,  0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));
	memset(&szFilter,    0, sizeof(szFilter));

	strcpy(szFilter, _("PCSX State Format"));
	strcatz(szFilter, "*.*");

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= gApp.hWnd;
	ofn.lpstrFilter			= szFilter;
	ofn.lpstrCustomFilter		= NULL;
	ofn.nMaxCustFilter		= 0;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szFileName;
	ofn.nMaxFile			= MAXPATHLEN;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= MAXPATHLEN;
	ofn.lpstrTitle			= NULL;
	ofn.lpstrDefExt			= NULL;
	ofn.Flags			= OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if (GetOpenFileName ((LPOPENFILENAME)&ofn)) {
		char Text[256];
		int ret;

		SetMenu(gApp.hWnd, NULL);
		OpenPlugins(gApp.hWnd);

		ret = LoadState(szFileName);
		if (ret == 0)
			 sprintf(Text, _("*PCSX*: Loaded State %s"), szFileName);
		else sprintf(Text, _("*PCSX*: Error Loading State %s"), szFileName);
		GPU_displayText(Text);

		Running = 1;
		psxCpu->Execute();
	}
} 

void OnStates_SaveOther() {
	OPENFILENAME ofn;
	char szFileName[MAXPATHLEN];
	char szFileTitle[MAXPATHLEN];
	char szFilter[256];

	memset(&szFileName,  0, sizeof(szFileName));
	memset(&szFileTitle, 0, sizeof(szFileTitle));
	memset(&szFilter,    0, sizeof(szFilter));

	strcpy(szFilter, _("PCSX State Format"));
	strcatz(szFilter, "*.*");

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= gApp.hWnd;
	ofn.lpstrFilter			= szFilter;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nMaxCustFilter		= 0;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szFileName;
	ofn.nMaxFile			= MAXPATHLEN;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= MAXPATHLEN;
	ofn.lpstrTitle			= NULL;
	ofn.lpstrDefExt			= NULL;
	ofn.Flags				= OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

	if (GetOpenFileName ((LPOPENFILENAME)&ofn)) {
		char Text[256];
		int ret;

		SetMenu(gApp.hWnd, NULL);
		OpenPlugins(gApp.hWnd);

		ret = SaveState(szFileName);
		if (ret == 0)
			 sprintf(Text, _("*PCSX*: Saved State %s"), szFileName);
		else sprintf(Text, _("*PCSX*: Error Saving State %s"), szFileName);
		GPU_displayText(Text);

		Running = 1;
		psxCpu->Execute();
	}
} 

LRESULT WINAPI MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	char File[256];
	PAINTSTRUCT ps;

	switch (msg) {
		case WM_CREATE:
			hBm = LoadBitmap(gApp.hInstance, MAKEINTRESOURCE(MAIN_LOGO));
			GetObject(hBm, sizeof(BITMAP), (LPVOID)&bm);
			hDC = GetDC(hWnd);
			hdcmem = CreateCompatibleDC(hDC);
			ReleaseDC(hWnd, hDC);
			break;

		case WM_PAINT:
			hDC = BeginPaint(hWnd, &ps);
			SelectObject(hdcmem, hBm);
			if (!Running) BitBlt(hDC, 0, 0, bm.bmWidth, bm.bmHeight, hdcmem, 0, 0, SRCCOPY);
			EndPaint(hWnd, &ps);
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case ID_FILE_EXIT:
					SysClose();
					PostQuitMessage(0);
					exit(0);
					return TRUE;

				case ID_FILE_RUN_CD:
					SetIsoFile(NULL);
					SetMenu(hWnd, NULL);
					LoadPlugins();
					if (OpenPlugins(hWnd) == -1) {
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					if (CheckCdrom() == -1) {
						ClosePlugins();
						RestoreWindow();
						SysMessage(_("The CD does not appear to be a valid Playstation CD"));
						return TRUE;
					}
					
					// Auto-detect: region first, then rcnt reset
					SysReset();

					if (LoadCdrom() == -1) {
						ClosePlugins();
						RestoreWindow();
						SysMessage(_("Could not load CD-ROM!"));
						return TRUE;
					}
					ShowCursor(FALSE);
					Running = 1;
					psxCpu->Execute();
					return TRUE;

				case ID_FILE_RUNBIOS:
					if (strcmp(Config.Bios, "HLE") == 0) {
						SysMessage(_("Running BIOS is not supported with Internal HLE Bios."));
						return TRUE;
					}
					SetIsoFile(NULL);
					SetMenu(hWnd, NULL);
					LoadPlugins();
					if (OpenPlugins(hWnd) == -1) {
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					ShowCursor(FALSE);
					SysReset();
					CdromId[0] = '\0';
					CdromLabel[0] = '\0';
					Running = 1;
					psxCpu->Execute();
					return TRUE;

				case ID_FILE_RUN_ISO:
					if (!Open_Iso_Proc(File)) return TRUE;
					SetIsoFile(File);
					SetMenu(hWnd, NULL);
					LoadPlugins();
					if (OpenPlugins(hWnd) == -1) {
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					if (CheckCdrom() == -1) {
						ClosePlugins();
						RestoreWindow();
						SysMessage(_("The CD does not appear to be a valid Playstation CD"));
						return TRUE;
					}

					// Auto-detect: region first, then rcnt reset
					SysReset();
					
					if (LoadCdrom() == -1) {
						ClosePlugins();
						RestoreWindow();
						SysMessage(_("Could not load CD-ROM!"));
						return TRUE;
					}
					ShowCursor(FALSE);
					Running = 1;
					psxCpu->Execute();
					return TRUE;

				case ID_FILE_RUN_EXE:
					if (!Open_File_Proc(File)) return TRUE;
					SetIsoFile(NULL);
					SetMenu(hWnd, NULL);
					LoadPlugins();
					if (OpenPlugins(hWnd) == -1) {
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					CheckCdrom();

					// Auto-detect: region first, then rcnt reset
					SysReset();
					
					Load(File);
					Running = 1;
					psxCpu->Execute();
					return TRUE;

				case ID_FILE_STATES_LOAD_SLOT1: States_Load(0); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT2: States_Load(1); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT3: States_Load(2); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT4: States_Load(3); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT5: States_Load(4); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT6: States_Load(5); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT7: States_Load(6); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT8: States_Load(7); return TRUE;
				case ID_FILE_STATES_LOAD_SLOT9: States_Load(8); return TRUE;
				case ID_FILE_STATES_LOAD_OTHER: OnStates_LoadOther(); return TRUE;

				case ID_FILE_STATES_SAVE_SLOT1: States_Save(0); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT2: States_Save(1); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT3: States_Save(2); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT4: States_Save(3); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT5: States_Save(4); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT6: States_Save(5); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT7: States_Save(6); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT8: States_Save(7); return TRUE;
				case ID_FILE_STATES_SAVE_SLOT9: States_Save(8); return TRUE;
				case ID_FILE_STATES_SAVE_OTHER: OnStates_SaveOther(); return TRUE;

				case ID_FILE_RUN_NOGUI:
					SetMenu(hWnd, NULL);
					LoadPlugins();
					if (OpenPlugins(hWnd) == -1) {
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					if (CheckCdrom() == -1) {
						fprintf(stderr, _("The CD does not appear to be a valid Playstation CD"));
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}

					// Auto-detect: region first, then rcnt reset
					SysReset();
					
					if (LoadCdrom() == -1) {
						fprintf(stderr, _("Could not load CD-ROM!"));
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					ShowCursor(FALSE);
					Running = 1;
					psxCpu->Execute();
					return TRUE;

				case ID_EMULATOR_RUN:
					SetMenu(hWnd, NULL);
					OpenPlugins(hWnd);
					ShowCursor(FALSE);
					Running = 1;
					CheatSearchBackupMemory();
					psxCpu->Execute();
					return TRUE;

				case ID_EMULATOR_RESET:
					SetMenu(hWnd, NULL);
					OpenPlugins(hWnd);
					CheckCdrom();

					// Auto-detect: region first, then rcnt reset
					SysReset();
					
					LoadCdrom();
					ShowCursor(FALSE);
					Running = 1;
					psxCpu->Execute();
					return TRUE;

				case ID_EMULATOR_SWITCH_ISO:
					if (!Open_Iso_Proc(File)) return TRUE;
					//Pause emulation
                                        SetIsoFile(File);
                                        SetCdOpenCaseTime(time(NULL) + 2);
                                        // Resume Emulation
					SetMenu(hWnd, NULL);
					if (OpenPlugins(hWnd) == -1) {
						ClosePlugins();
						RestoreWindow();
						return TRUE;
					}
					ShowCursor(FALSE);
					Running = 1;
					SetCdOpenCaseTime(time(NULL) + 2);
					CheatSearchBackupMemory();
					psxCpu->Execute();
					return TRUE;

				case ID_CONFIGURATION_GRAPHICS:
					if (GPU_configure) GPU_configure();
					return TRUE;

				case ID_CONFIGURATION_SOUND:
					if (SPU_configure) SPU_configure();
					return TRUE;

				case ID_CONFIGURATION_CONTROLLERS:
					if (PAD1_configure) PAD1_configure();
					if (strcmp(Config.Pad1, Config.Pad2)) if (PAD2_configure) PAD2_configure();
					return TRUE;

				case ID_CONFIGURATION_CDROM:
				    if (CDR_configure) CDR_configure();
					return TRUE;

				case ID_CONFIGURATION_NETPLAY:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_NETPLAY), hWnd, (DLGPROC)ConfigureNetPlayDlgProc);
					return TRUE;

				case ID_CONFIGURATION_MEMORYCARDMANAGER:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_MCDCONF), hWnd, (DLGPROC)ConfigureMcdsDlgProc);
					return TRUE;

				case ID_CONFIGURATION_CPU:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CPUCONF), hWnd, (DLGPROC)ConfigureCpuDlgProc);
					return TRUE;

				case ID_CONFIGURATION:
					ConfigurePlugins(hWnd);
					return TRUE;

				case ID_CONFIGURATION_CHEATLIST:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CHEATLIST), hWnd, (DLGPROC)CheatDlgProc);
					break;

				case ID_CONFIGURATION_CHEATSEARCH:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(IDD_CHEATSEARCH), hWnd, (DLGPROC)CheatSearchDlgProc);
					break;

				case ID_HELP_ABOUT:
					DialogBox(gApp.hInstance, MAKEINTRESOURCE(ABOUT_DIALOG), hWnd, (DLGPROC)AboutDlgProc);
					return TRUE;

				default:
#ifdef ENABLE_NLS
					if (LOWORD(wParam) >= ID_LANGS && LOWORD(wParam) <= (ID_LANGS + langsMax)) {
						AccBreak = 1;
						DestroyWindow(gApp.hWnd);
						ChangeLanguage(langs[LOWORD(wParam) - ID_LANGS].lang);
						CreateMainWindow(SW_NORMAL);
						return TRUE;
					}
#endif
					break;
			}
			break;

		case WM_SYSKEYDOWN:
			if (wParam != VK_F10)
				return DefWindowProc(hWnd, msg, wParam, lParam);
		case WM_KEYDOWN:
			PADhandleKey(wParam);
			return TRUE;

		case WM_DESTROY:
			if (!AccBreak) {
				if (Running) ClosePlugins();
				SysClose();
				PostQuitMessage(0);
				exit(0);
			}
			else AccBreak = 0;

			DeleteObject(hBm);
			DeleteDC(hdcmem);
			return TRUE;

		case WM_QUIT:
			exit(0);
			break;

		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	return FALSE;
}

HWND mcdDlg;
McdBlock Blocks[2][15];
int IconC[2][15];
HIMAGELIST Iiml[2];
HICON eICON;

void CreateListView(int idc) {
	HWND List;
	LV_COLUMN col;

	List = GetDlgItem(mcdDlg, idc);

	col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	col.fmt  = LVCFMT_LEFT;

	col.pszText  = _("Title");
	col.cx       = 170;
	col.iSubItem = 0;

	ListView_InsertColumn(List, 0, &col);

	col.pszText  = _("Status");
	col.cx       = 50;
	col.iSubItem = 1;

	ListView_InsertColumn(List, 1, &col);

	col.pszText  = _("Game ID");
	col.cx       = 90;
	col.iSubItem = 2;

	ListView_InsertColumn(List, 2, &col);

	col.pszText  = _("Game");
	col.cx       = 80;
	col.iSubItem = 3;

	ListView_InsertColumn(List, 3, &col);
}

int GetRGB() {
    HDC scrDC, memDC;
    HBITMAP oldBmp = NULL; 
    HBITMAP curBmp = NULL;
    COLORREF oldColor;
    COLORREF curColor = RGB(255,255,255);
    int i, R, G, B;

    R = G = B = 1;
 
    scrDC = CreateDC("DISPLAY", NULL, NULL, NULL);
    memDC = CreateCompatibleDC(NULL); 
    curBmp = CreateCompatibleBitmap(scrDC, 1, 1);    
    oldBmp = (HBITMAP)SelectObject(memDC, curBmp);
        
    for (i = 255; i >= 0; --i) {
        oldColor = curColor;
        curColor = SetPixel(memDC, 0, 0, RGB(i, i, i));
 
        if (GetRValue(curColor) < GetRValue(oldColor)) ++R; 
        if (GetGValue(curColor) < GetGValue(oldColor)) ++G;
        if (GetBValue(curColor) < GetBValue(oldColor)) ++B;
    }
 
    DeleteObject(oldBmp);
    DeleteObject(curBmp);
    DeleteDC(scrDC);
    DeleteDC(memDC);
 
    return (R * G * B);
}
 
HICON GetIcon(short *icon) {
    ICONINFO iInfo;
    HDC hDC;
    char mask[16*16];
    int x, y, c, Depth;
  
    hDC = CreateIC("DISPLAY",NULL,NULL,NULL);
    Depth=GetDeviceCaps(hDC, BITSPIXEL);
    DeleteDC(hDC);
 
    if (Depth == 16) {
        if (GetRGB() == (32 * 32 * 32))        
            Depth = 15;
    }

    for (y=0; y<16; y++) {
        for (x=0; x<16; x++) {
            c = icon[y*16+x];
            if (Depth == 15 || Depth == 32)
				c = ((c&0x001f) << 10) | 
					((c&0x7c00) >> 10) | 
					((c&0x03e0)      );
			else
                c = ((c&0x001f) << 11) |
					((c&0x7c00) >>  9) |
					((c&0x03e0) <<  1);

            icon[y*16+x] = c;
        }
    }    

    iInfo.fIcon = TRUE;
    memset(mask, 0, 16*16);
    iInfo.hbmMask  = CreateBitmap(16, 16, 1, 1, mask);
    iInfo.hbmColor = CreateBitmap(16, 16, 1, 16, icon); 
 
    return CreateIconIndirect(&iInfo);
}

HICON hICON[2][3][15];
int aIover[2];                        
int ani[2];
 
void LoadMcdItems(int mcd, int idc) {
    HWND List = GetDlgItem(mcdDlg, idc);
    LV_ITEM item;
    HIMAGELIST iml = Iiml[mcd-1];
    int i, j;
    HICON hIcon;
    McdBlock *Info;
 
    aIover[mcd-1]=0;
    ani[mcd-1]=0;
 
    ListView_DeleteAllItems(List);

    for (i=0; i<15; i++) {
  
        item.mask      = LVIF_TEXT | LVIF_IMAGE;
        item.iItem       = i;
        item.iImage    = i;
        item.pszText  = LPSTR_TEXTCALLBACK;
        item.iSubItem = 0;
 
        IconC[mcd-1][i] = 0;
        Info = &Blocks[mcd-1][i];
 
        if ((Info->Flags & 0xF) == 1 && Info->IconCount != 0) {
            hIcon = GetIcon(Info->Icon);   
 
            if (Info->IconCount > 1) {
                for(j = 0; j < 3; j++)
                    hICON[mcd-1][j][i]=hIcon;
            }
        } else {
            hIcon = eICON; 
        }
 
        ImageList_ReplaceIcon(iml, -1, hIcon);
        ListView_InsertItem(List, &item);
    } 
}

void UpdateMcdItems(int mcd, int idc) {
    HWND List = GetDlgItem(mcdDlg, idc);
    LV_ITEM item;
    HIMAGELIST iml = Iiml[mcd-1];
    int i, j;
    McdBlock *Info;
    HICON hIcon;
 
    aIover[mcd-1]=0;
    ani[mcd-1]=0;
  
    for (i=0; i<15; i++) { 
 
        item.mask     = LVIF_TEXT | LVIF_IMAGE;
        item.iItem    = i;
        item.iImage   = i;
        item.pszText  = LPSTR_TEXTCALLBACK;
        item.iSubItem = 0;
 
        IconC[mcd-1][i] = 0; 
        Info = &Blocks[mcd-1][i];
 
        if ((Info->Flags & 0xF) == 1 && Info->IconCount != 0) {
            hIcon = GetIcon(Info->Icon);   
 
            if (Info->IconCount > 1) { 
                for(j = 0; j < 3; j++)
                    hICON[mcd-1][j][i]=hIcon;
            }
        } else { 
            hIcon = eICON; 
        }
              
        ImageList_ReplaceIcon(iml, i, hIcon);
        ListView_SetItem(List, &item);
    } 
    ListView_Update(List, -1);
}
 
void McdListGetDispInfo(int mcd, int idc, LPNMHDR pnmh) {
	LV_DISPINFO *lpdi = (LV_DISPINFO *)pnmh;
	McdBlock *Info;
	char buf[256];
	static char buftitle[256];

	Info = &Blocks[mcd - 1][lpdi->item.iItem];

	switch (lpdi->item.iSubItem) {
		case 0:
			switch (Info->Flags & 0xF) {
				case 1:
					if (MultiByteToWideChar(932, 0, (LPCSTR)Info->sTitle, -1, (LPWSTR)buf, sizeof(buf)) == 0) {
						lpdi->item.pszText = Info->Title;
					} else if (WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)buf, -1, (LPSTR)buftitle, sizeof(buftitle), NULL, NULL) == 0) {
						lpdi->item.pszText = Info->Title;
					} else {
						lpdi->item.pszText = buftitle;
					}
					break;
				case 2:
					lpdi->item.pszText = _("mid link block");
					break;
				case 3:
					lpdi->item.pszText = _("terminiting link block");
					break;
			}
			break;
		case 1:
			if ((Info->Flags & 0xF0) == 0xA0) {
				if ((Info->Flags & 0xF) >= 1 &&
					(Info->Flags & 0xF) <= 3) {
					lpdi->item.pszText = _("Deleted");
				} else lpdi->item.pszText = _("Free");
			} else if ((Info->Flags & 0xF0) == 0x50)
				lpdi->item.pszText = _("Used");
			else { lpdi->item.pszText = _("Free"); }
			break;
		case 2:
			if((Info->Flags & 0xF)==1)
				lpdi->item.pszText = Info->ID;
			break;
		case 3:
			if((Info->Flags & 0xF)==1)
				lpdi->item.pszText = Info->Name;
			break;
	}
}

void McdListNotify(int mcd, int idc, LPNMHDR pnmh) {
	switch (pnmh->code) {
		case LVN_GETDISPINFO: McdListGetDispInfo(mcd, idc, pnmh); break;
	}
}

void UpdateMcdDlg() {
	int i;

	for (i=1; i<16; i++) GetMcdBlockInfo(1, i, &Blocks[0][i-1]);
	for (i=1; i<16; i++) GetMcdBlockInfo(2, i, &Blocks[1][i-1]);
	UpdateMcdItems(1, IDC_LIST1);
	UpdateMcdItems(2, IDC_LIST2);
}

void LoadMcdDlg() {
	int i;

	for (i=1; i<16; i++) GetMcdBlockInfo(1, i, &Blocks[0][i-1]);
	for (i=1; i<16; i++) GetMcdBlockInfo(2, i, &Blocks[1][i-1]);
	LoadMcdItems(1, IDC_LIST1);
	LoadMcdItems(2, IDC_LIST2);
}

void UpdateMcdIcon(int mcd, int idc) {
    HWND List = GetDlgItem(mcdDlg, idc);
    HIMAGELIST iml = Iiml[mcd-1];
    int i;
    McdBlock *Info;
    int *count; 
 
    if(!aIover[mcd-1]) {
        ani[mcd-1]++; 
 
        for (i=0; i<15; i++) {
            Info = &Blocks[mcd-1][i];
            count = &IconC[mcd-1][i];
 
            if ((Info->Flags & 0xF) != 1) continue;
            if (Info->IconCount <= 1) continue;
 
            if (*count < Info->IconCount) {
                (*count)++;
                aIover[mcd-1]=0;
 
                if(ani[mcd-1] <= (Info->IconCount-1))  // last frame and below...
                    hICON[mcd-1][ani[mcd-1]][i] = GetIcon(&Info->Icon[(*count)*16*16]);
            } else {
                aIover[mcd-1]=1;
            }
        }

    } else { 
 
        if (ani[mcd-1] > 1) ani[mcd-1] = 0;  // 1st frame
        else ani[mcd-1]++;                       // 2nd, 3rd frame
 
        for(i=0;i<15;i++) {
            Info = &Blocks[mcd-1][i];
 
            if (((Info->Flags & 0xF) == 1) && (Info->IconCount > 1))
                ImageList_ReplaceIcon(iml, i, hICON[mcd-1][ani[mcd-1]][i]);
        }
        InvalidateRect(List,  NULL, FALSE);
    }
}

static int copy = 0, copymcd = 0;
//static int listsel = 0;

BOOL CALLBACK ConfigureMcdsDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	char str[256];
	LPBYTE lpAND, lpXOR;
	LPBYTE lpA, lpX;
	int i, j;

	switch(uMsg) {
		case WM_INITDIALOG:
			mcdDlg = hW;

			SetWindowText(hW, _("Memcard Manager"));

			Button_SetText(GetDlgItem(hW, IDOK),        _("OK"));
			Button_SetText(GetDlgItem(hW, IDCANCEL),    _("Cancel"));
			Button_SetText(GetDlgItem(hW, IDC_MCDSEL1), _("Select Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_FORMAT1), _("Format Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_RELOAD1), _("Reload Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_MCDSEL2), _("Select Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_FORMAT2), _("Format Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_RELOAD2), _("Reload Mcd"));
			Button_SetText(GetDlgItem(hW, IDC_COPYTO2), _("-> Copy ->"));
			Button_SetText(GetDlgItem(hW, IDC_COPYTO1), _("<- Copy <-"));
			Button_SetText(GetDlgItem(hW, IDC_PASTE),   _("Paste"));
			Button_SetText(GetDlgItem(hW, IDC_DELETE1), _("<- Un/Delete"));
			Button_SetText(GetDlgItem(hW, IDC_DELETE2), _("Un/Delete ->"));
 
			Static_SetText(GetDlgItem(hW, IDC_FRAMEMCD1), _("Memory Card 1"));
			Static_SetText(GetDlgItem(hW, IDC_FRAMEMCD2), _("Memory Card 2"));

			lpA=lpAND=(LPBYTE)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(16*16));
			lpX=lpXOR=(LPBYTE)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(16*16));

			for(i=0;i<16;i++)
			{
				for(j=0;j<16;j++)
				{
					*lpA++=0xff;
					*lpX++=0;
				}
			}
			eICON=CreateIcon(gApp.hInstance,16,16,1,1,lpAND,lpXOR);

			HeapFree(GetProcessHeap(),0,lpAND);
			HeapFree(GetProcessHeap(),0,lpXOR);

			if (!strlen(Config.Mcd1)) strcpy(Config.Mcd1, "memcards\\Mcd001.mcr");
			if (!strlen(Config.Mcd2)) strcpy(Config.Mcd2, "memcards\\Mcd002.mcr");
			Edit_SetText(GetDlgItem(hW,IDC_MCD1), Config.Mcd1);
			
