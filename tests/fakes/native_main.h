/* Declarations needed to compile main.c's native orchestration on the host.
 * Socket tests retain real POSIX networking; unused platform code is discarded. */
#define SYSCLOCK_ARM11 268123480ULL
typedef struct { int16_t dx,dy; } circlePosition;
typedef struct { uint16_t px,py; } touchPosition;
typedef enum { APTHOOK_ONSUSPEND, APTHOOK_ONSLEEP, APTHOOK_ONEXIT } APT_HookType;
typedef struct { int unused; } aptHookCookie;
typedef struct { int unused; } SwkbdState;
enum { GFX_TOP, GFX_BOTTOM, SWKBD_TYPE_NUMPAD, SWKBD_TYPE_NORMAL,
       SWKBD_PASSWORD_NONE, SWKBD_PASSWORD_HIDE, SWKBD_BUTTON_RIGHT };
enum { KEY_A=1, KEY_B=2, KEY_X=4, KEY_Y=8, KEY_L=16, KEY_R=32,
       KEY_ZR=64, KEY_ZL=128, KEY_SELECT=256, KEY_START=512, KEY_DUP=1024,
       KEY_DRIGHT=2048, KEY_DDOWN=4096, KEY_DLEFT=8192, KEY_TOUCH=16384 };
uint64_t svcGetSystemTick(void);
void svcSleepThread(int64_t ns);
void hidScanInput(void);
uint32_t hidKeysHeld(void);
uint32_t hidKeysDown(void);
void hidCircleRead(circlePosition *p);
void hidCstickRead(circlePosition *p);
void hidTouchRead(touchPosition *p);
void swkbdInit(SwkbdState *s,int type,int buttons,int length);
void swkbdSetHintText(SwkbdState *s,const char *text);
void swkbdSetInitialText(SwkbdState *s,const char *text);
void swkbdSetFeatures(SwkbdState *s,unsigned features);
void swkbdSetPasswordMode(SwkbdState *s,int mode);
int swkbdInputText(SwkbdState *s,char *out,size_t capacity);
void gfxInitDefault(void);
void gfxExit(void);
void consoleInit(int screen,void *console);
void gspWaitForVBlank(void);
bool aptMainLoop(void);
void aptHook(aptHookCookie *cookie,void (*callback)(APT_HookType,void *),void *arg);
void aptUnhook(aptHookCookie *cookie);
Result socInit(void *buffer,unsigned size);
void socExit(void);
Result acInit(void);
void acExit(void);
