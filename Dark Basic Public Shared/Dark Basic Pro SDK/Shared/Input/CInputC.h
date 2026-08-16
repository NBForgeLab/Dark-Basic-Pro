#ifndef _CINPUT_H_
#define _CINPUT_H_

//////////////////////////////////////////////////////////////////////////////////
// INCLUDES / LIBS ///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

// Removed DirectInput8 APIs, using Raw Input & XInput.
// All DirectInput headers, libraries, and runtime APIs are completely purged.
#include <windows.h>
#include <xinput.h>

// Mock structures to maintain binary/ABI layout compatibility

#ifndef DIJOYSTATE2
struct DIJOYSTATE2 {
    LONG    lX;
    LONG    lY;
    LONG    lZ;
    LONG    lRx;
    LONG    lRy;
    LONG    lRz;
    LONG    rglSlider[2];
    DWORD   rgdwPOV[4];
    BYTE    rgbButtons[128];
    LONG    rglVSlider[2];
};
#endif

#ifndef DIMOUSESTATE
struct DIMOUSESTATE {
    LONG    lX;
    LONG    lY;
    LONG    lZ;
    BYTE    rgbButtons[4];
};
#endif

#ifndef DIDEVCAPS
struct DIDEVCAPS {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwDevType;
    DWORD dwAxes;
    DWORD dwButtons;
    DWORD dwPOVs;
};
#endif

// Legacy Cooperative Level flags
#define DISCL_FOREGROUND    0x00000001

// DirectInput Keyboard Scan Code Constants (Standard Keyboard Set 1 Make Codes)
#define DIK_ESCAPE          0x01
#define DIK_1               0x02
#define DIK_2               0x03
#define DIK_3               0x04
#define DIK_4               0x05
#define DIK_5               0x06
#define DIK_6               0x07
#define DIK_7               0x08
#define DIK_8               0x09
#define DIK_9               0x0A
#define DIK_0               0x0B
#define DIK_MINUS           0x0C    /* - on main keyboard */
#define DIK_EQUALS          0x0D    /* = on main keyboard */
#define DIK_BACK            0x0E    /* backspace */
#define DIK_TAB             0x0F    /* tab */
#define DIK_Q               0x10
#define DIK_W               0x11
#define DIK_E               0x12
#define DIK_R               0x13
#define DIK_T               0x14
#define DIK_Y               0x15
#define DIK_U               0x16
#define DIK_I               0x17
#define DIK_O               0x18
#define DIK_P               0x19
#define DIK_LBRACKET        0x1A
#define DIK_RBRACKET        0x1B
#define DIK_RETURN          0x1C    /* Enter on main keyboard */
#define DIK_LCONTROL        0x1D
#define DIK_A               0x1E
#define DIK_S               0x1F
#define DIK_D               0x20
#define DIK_F               0x21
#define DIK_G               0x22
#define DIK_H               0x23
#define DIK_J               0x24
#define DIK_K               0x25
#define DIK_L               0x26
#define DIK_SEMICOLON       0x27
#define DIK_APOSTROPHE      0x28
#define DIK_GRAVE           0x29    /* accent grave */
#define DIK_LSHIFT          0x2A
#define DIK_BACKSLASH       0x2B
#define DIK_Z               0x2C
#define DIK_X               0x2D
#define DIK_C               0x2E
#define DIK_V               0x2F
#define DIK_B               0x30
#define DIK_N               0x31
#define DIK_M               0x32
#define DIK_COMMA           0x33
#define DIK_PERIOD          0x34    /* . on main keyboard */
#define DIK_SLASH           0x35    /* / on main keyboard */
#define DIK_RSHIFT          0x36
#define DIK_MULTIPLY        0x37    /* * on numpad */
#define DIK_LMENU           0x38    /* left Alt */
#define DIK_SPACE           0x39
#define DIK_NUMPAD7         0x47
#define DIK_NUMPAD8         0x48
#define DIK_NUMPAD9         0x49
#define DIK_SUBTRACT        0x4A    /* - on numpad */
#define DIK_NUMPAD4         0x4B
#define DIK_NUMPAD5         0x4C
#define DIK_NUMPAD6         0x4D
#define DIK_ADD             0x4E    /* + on numpad */
#define DIK_NUMPAD1         0x4F
#define DIK_NUMPAD2         0x50
#define DIK_NUMPAD3         0x51
#define DIK_NUMPAD0         0x52
#define DIK_DECIMAL         0x53    /* . on numpad */
#define DIK_DIVIDE          0xB5    /* / on numpad */
#define DIK_RCONTROL        0x9D
#define DIK_UP              0xC8    /* UpArrow on arrow keypad */
#define DIK_LEFT            0xCB    /* LeftArrow on arrow keypad */
#define DIK_RIGHT           0xCD    /* RightArrow on arrow keypad */
#define DIK_DOWN            0xD0    /* DownArrow on arrow keypad */

void MapXInputToDIJoyState(const XINPUT_STATE& state, DIJOYSTATE2& joyState);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////
// DEFINES ///////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

#ifndef DARKSDK_COMPILE
	#define DARKSDK __declspec ( dllexport )
	#define DBPRO_GLOBAL 
#else
	#define DARKSDK static
	#define DBPRO_GLOBAL static
#endif

#define SAFE_DELETE( p )       { if ( p ) { delete ( p );       ( p ) = NULL; } }
#define SAFE_RELEASE( p )      { if ( p ) { ( p )->Release ( ); ( p ) = NULL; } }
#define SAFE_DELETE_ARRAY( p ) { if ( p ) { delete [ ] ( p );   ( p ) = NULL; } }
#define KEYDOWN(name, key) (name[key] & 0x80) 

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS /////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

DARKSDK void			SetupKeyboardEx				( DWORD dwForeOrBackGround );
DARKSDK void 			SetupKeyboard      			( void );
DARKSDK void			SetupMouseEx				( DWORD dwForeOrBackGround );
DARKSDK void 			SetupMouse         			( void );
DARKSDK void 			SetupJoystick      			( void );
DARKSDK void 			SetupForceFeedback 			( void );

DARKSDK void 			UpdateKeyboard     			( void );
DARKSDK void 			UpdateMouse					( void );



DARKSDK char			InKey      					( void );
DARKSDK void			ClearData 					( void );

#ifdef DARKSDK_COMPILE
		void			ConstructorInput 			( HINSTANCE );
		void 			DestructorInput  			( void );
		void 			SetErrorHandlerInput		( LPVOID pErrorHandlerPtr );
		void 			PassCoreDataInput			( LPVOID pGlobPtr );
		void 			RefreshD3DInput 			( int iMode );
#endif

DARKSDK void 			Constructor 				( HINSTANCE );
DARKSDK void 			Destructor  				( void );
DARKSDK void 			SetErrorHandler 			( LPVOID pErrorHandlerPtr );
DARKSDK void 			PassCoreData				( LPVOID pGlobPtr );
DARKSDK void 			RefreshD3D 					( int iMode );

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS //////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

// entry commands
DARKSDK void 			ClearEntryBuffer 					( void );					// clear holding buffer
DARKSDK DWORD_PTR GetEntryEx( DWORD_PTR pDestStr, int iAutoBackSpaceMode );
DARKSDK DWORD_PTR GetEntry( DWORD_PTR pDestStr );			// get from windows entry buffer

// keyboard commands
DARKSDK int 			UpKey      							( void );					// is the up key being pressed
DARKSDK int 			DownKey    							( void );					// is the down key being pressed
DARKSDK int 			LeftKey   							( void );					// is the left key being pressed
DARKSDK int 			RightKey   							( void );					// is the right key being pressed
DARKSDK int 			ControlKey 							( void );					// is the control key being pressed
DARKSDK int 			ShiftKey   							( void );					// is the shift key being pressed
DARKSDK int 			SpaceKey   							( void );					// is the space key being pressed
DARKSDK int 			EscapeKey  							( void );					// is the escape key being pressed
DARKSDK int 			ReturnKey  							( void );					// is the return key being pressed
DARKSDK int 			KeyState   							( int iKey );				// return true if the key is being pressed

// mike - 230107 - new function to get state, useful for caps lock etc
DARKSDK int				GetKeyStateEx						( int iKey );

DARKSDK int 			ScanCode   							( void );					// return the scan code of the current key press

// misc commands
DARKSDK void 			WriteToClipboard					( LPSTR );				// write to clkipboard
DARKSDK void 			SetRegistryHKEY						( int );
DARKSDK void 			WriteToRegistry						( LPSTR, LPSTR, int );		// write to registry
DARKSDK DWORD_PTR GetClipboard( DWORD_PTR );					// get from clipboard
DARKSDK int 			GetRegistry							( LPSTR, LPSTR );				// get from registry
DARKSDK void 			WriteToRegistryS					( LPSTR pfolder, LPSTR valuekey, DWORD_PTR pString );
DARKSDK void 			WriteToRegistrySL					( LPSTR pfolder, LPSTR valuekey, DWORD_PTR pString, int iCurrentUserMode );
DARKSDK DWORD_PTR GetRegistryS( DWORD_PTR pDestStr, LPSTR pfolder, LPSTR valuekey );
DARKSDK DWORD_PTR GetRegistrySL( DWORD_PTR pDestStr, LPSTR pfolder, LPSTR valuekey, int iCurrentUserMode );

// mouse commands
DARKSDK void 			HideMouse     						( void );	// hide
DARKSDK void 			ShowMouse     						( void );	// show
DARKSDK void 			PositionMouse 						( int, int );// position
DARKSDK void 			ChangeMouse   						( int );		// change
DARKSDK void			ChangeMouse							( int, int );	// 13/01/11 - touch friendly flag
DARKSDK int  			GetMouseX     						( void );	// get x position
DARKSDK int  			GetMouseY     						( void );	// get y position
DARKSDK int  			GetMouseZ     						( void );	// get z position ( mouse wheel )
DARKSDK int  			GetMouseClick 						( void );	// get which mouse button has been clicked
DARKSDK int  			GetMouseMoveX 						( void );	// get the relative x movement
DARKSDK int  			GetMouseMoveY 						( void );	// get the relative y movement
DARKSDK int  			GetMouseMoveZ 						( void );	// get the relative z movement

// joystick commands
DARKSDK int 			JoystickUp 							( void );
DARKSDK int 			JoystickDown 						( void );
DARKSDK int 			JoystickLeft 						( void );
DARKSDK int 			JoystickRight 						( void );
DARKSDK int 			JoystickX 							( void );
DARKSDK int 			JoystickY 							( void );
DARKSDK int 			JoystickZ 							( void );
DARKSDK int 			JoystickFireA 						( void );
DARKSDK int 			JoystickFireB 						( void );
DARKSDK int 			JoystickFireC 						( void );
DARKSDK int 			JoystickFireD 						( void );
DARKSDK int 			JoystickFireXL 						( int iButton );
DARKSDK int 			JoystickSliderA 					( void );
DARKSDK int 			JoystickSliderB 					( void );
DARKSDK int 			JoystickSliderC 					( void );
DARKSDK int 			JoystickSliderD 					( void );
DARKSDK int 			JoystickTwistX 						( void );
DARKSDK int 			JoystickTwistY 						( void );
DARKSDK int 			JoystickTwistZ 						( void );
DARKSDK int 			JoystickHatAngle 					( int iHatID );

// force commands
DARKSDK void 			ForceUp 							( int iMagnitude );
DARKSDK void 			ForceDown							( int iMagnitude );
DARKSDK void 			ForceLeft 							( int iMagnitude );
DARKSDK void 			ForceRight 							( int iMagnitude );
DARKSDK void 			ForceAngle							( int iMagnitude, int iAngle, int iDuration );
DARKSDK void 			ForceChainsaw						( int iMagnitude, int iDuration );
DARKSDK void 			ForceShoot							( int iMagnitude, int iDuration );
DARKSDK void 			ForceImpact							( int iMagnitude, int iDuration );
DARKSDK void 			ForceNoEffect						( void );
DARKSDK void 			ForceWaterEffect					( int iMagnitude, int iDuration );
DARKSDK void 			ForceAutoCenterOn					( void );
DARKSDK void 			ForceAutoCenterOff					( void );

// control device commands
DARKSDK void 			PerformChecklistControlDevices		( void );
DARKSDK void			SetControlDeviceEx					( DWORD_PTR pName, int iSubIndex );
DARKSDK void 			SetControlDevice					( DWORD_PTR pName );
DARKSDK void			SetControlDeviceIndex				( int iIndex );
DARKSDK DWORD_PTR GetControlDevice( DWORD_PTR pDestStr );
DARKSDK int 			ControlDeviceX						( void );
DARKSDK int 			ControlDeviceY						( void );
DARKSDK int 			ControlDeviceZ						( void );

#ifdef DARKSDK_COMPILE
		void 			dbClearEntryBuffer 					( void );
		char* 			dbGetEntry 							( void );

		int 			dbUpKey      						( void );
		int 			dbDownKey    						( void );
		int 			dbLeftKey   						( void );
		int 			dbRightKey   						( void );
		int 			dbControlKey 						( void );
		int 			dbShiftKey   						( void );
		int 			dbSpaceKey   						( void );
		int 			dbEscapeKey  						( void );
		int 			dbReturnKey  						( void );
		int 			dbKeyState   						( int iKey );
		int 			dbScanCode   						( void );

		void 			dbWriteToClipboard					( char* pString );
		void 			dbWriteToRegistry					( char* a, char* b, int c );
		char*			dbGetClipboard						( void );
		int 			dbGetRegistry						( LPSTR, LPSTR );
		// mike - 220107 - update last param of function
		void 			dbWriteToRegistryS					( LPSTR pfolder, LPSTR valuekey, char* pString );
		char* 			dbGetRegistryS						( LPSTR pfolder, LPSTR valuekey );

		void 			dbHideMouse     					( void );
		void 			dbShowMouse     					( void );
		void 			dbPositionMouse 					( int, int );
		void 			dbChangeMouse   					( int );
		int  			dbMouseX     						( void );
		int  			dbMouseY     						( void );
		int  			dbMouseZ     						( void );
		int  			dbMouseClick 						( void );
		int  			dbMouseMoveX 						( void );
		int  			dbMouseMoveY 						( void );
		int  			dbMouseMoveZ 						( void );

		int 			dbJoystickUp 						( void );
		int 			dbJoystickDown 						( void );
		int 			dbJoystickLeft 						( void );
		int 			dbJoystickRight 					( void );
		int 			dbJoystickX 						( void );
		int 			dbJoystickY 						( void );
		int 			dbJoystickZ 						( void );
		int 			dbJoystickFireA 					( void );
		int 			dbJoystickFireB 					( void );
		int 			dbJoystickFireC 					( void );
		int 			dbJoystickFireD 					( void );
		int 			dbJoystickFireXL 					( int iButton );
		int 			dbJoystickSliderA 					( void );
		int 			dbJoystickSliderB 					( void );
		int 			dbJoystickSliderC 					( void );
		int 			dbJoystickSliderD 					( void );
		int 			dbJoystickTwistX 					( void );
		int 			dbJoystickTwistY 					( void );
		int 			dbJoystickTwistZ 					( void );
		int 			dbJoystickHatAngle 					( int iHatID );

		void 			dbForceUp 							( int iMagnitude );
		void 			dbForceDown							( int iMagnitude );
		void 			dbForceLeft 						( int iMagnitude );
		void 			dbForceRight 						( int iMagnitude );
		void 			dbForceAngle						( int iMagnitude, int iAngle, int iDuration );
		void 			dbForceChainsaw						( int iMagnitude, int iDuration );
		void 			dbForceShoot						( int iMagnitude, int iDuration );
		void 			dbForceImpact						( int iMagnitude, int iDuration );
		void 			dbForceNoEffect						( void );
		void 			dbForceWaterEffect					( int iMagnitude, int iDuration );
		void 			dbForceAutoCenterOn					( void );
		void 			dbForceAutoCenterOff				( void );

		void 			dbPerformChecklistControlDevices	( void );
		void 			dbSetControlDevice					( char* pName );
		char* 			dbGetControlDevice 					( void );
		int 			dbControlDeviceX					( void );
		int 			dbControlDeviceY					( void );
		int 			dbControlDeviceZ					( void );
		void			dbClearDataInput 					( void );

		// lee - 300706 - GDK fixes
		char* 			dbEntry 							( void );
		void 			dbWriteStringToRegistry				( LPSTR pfolder, LPSTR valuekey, DWORD pString );
		char* 			dbGetRegistryString					( LPSTR pfolder, LPSTR valuekey );
		int 			dbJoystickFireX 					( int iButton );

#endif

//////////////////////////////////////////////////////////////////////////////////

#endif _CINPUT_H_