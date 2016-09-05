/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer
{
    // NOTE(casey): Pixels are alwasy 32-bits wide, Memory Order BB GG RR XX
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

// TODO(casey): This is a global for now.
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// Dynamically loaded
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(0);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(0);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibrary("xinput1_3.dll");
	if(XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
	}
}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	// NOTE: Load the library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	
	if(DSoundLibrary)
	{
		// NOTE: Get a DirectSound object - cooperative
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)
			GetProcAddress(DSoundLibrary, "DirectSoundCreate");
	
		// TODO: Check this works on XP, directsound8 or 7?
		LPDIRECTSOUND DirectSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16; // 16 bit audio like a CD
			WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;;
			WaveFormat.cbSize = 0;

			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// NOTE: Create a primary buffer
				// TODO: DSBCAPS_GLOBALFOCUS
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
						// NOTE: format set
						// OutputDebugStringA("
					}
					else
					{
						// TODO: diag/logging
					}
				}
						
			}
			else
			{
				// TODO: diagnostic/logging

			}

			
			// NOTE: Create a secondary buffer
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
			{
				if(SUCCEEDED(GlobalSecondaryBuffer->SetFormat(&WaveFormat)))
				{
					// NOTE: format set
				}
				else
				{
					// TODO: diag/logging
				}
			}
			BufferDescription.dwBufferBytes = BufferSize;
			
			// NOTE: Start it playin

		}	
	}
	else
	{
		// TODO: logging/diagnostic
	}
}


internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return(Result);
}

internal void
RenderWeirdGradient(win32_offscreen_buffer Buffer, int BlueOffset, int GreenOffset)
{
    // TODO(casey): Let's see what the optimizer does

    uint8 *Row = (uint8 *)Buffer.Memory;    
    for(int Y = 0;
        Y < Buffer.Height;
        ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int X = 0;
            X < Buffer.Width;
            ++X)
        {
            uint8 Blue = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);

            *Pixel++ = ((Green << 8) | Blue);
        }
        
        Row += Buffer.Pitch;
    }
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    // TODO(casey): Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    int BytesPerPixel = 4;

    // NOTE(casey): When the biHeight field is negative, this is the clue to
    // Windows to treat this bitmap as top-down, not bottom-up, meaning that
    // the first three bytes of the image are the color for the top left pixel
    // in the bitmap, not the bottom left!
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    // NOTE(casey): Thank you to Chris Hecker of Spy Party fame
    // for clarifying the deal with StretchDIBits and BitBlt!
    // No more DC for us.
    int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width*BytesPerPixel;

    // TODO(casey): Probably clear this to black
}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext,
                           int WindowWidth, int WindowHeight,
                           win32_offscreen_buffer Buffer)
{
    // TODO(casey): Aspect ratio correction
    // TODO(casey): Play with stretch modes
    StretchDIBits(DeviceContext,
                  /*
                  X, Y, Width, Height,
                  X, Y, Width, Height,
                  */
                  0, 0, WindowWidth, WindowHeight,
                  0, 0, Buffer.Width, Buffer.Height,
                  Buffer.Memory,
                  &Buffer.Info,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
                        UINT Message,
                        WPARAM WParam,
                        LPARAM LParam)
{       
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_CLOSE:
        {
            // TODO(casey): Handle this with a message to the user?
            GlobalRunning = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_DESTROY:
        {
            // TODO(casey): Handle this as an error - recreate window?
            GlobalRunning = false;
        } break;
        
				case WM_SYSKEYDOWN:
				case WM_SYSKEYUP:
				case WM_KEYDOWN:
				case WM_KEYUP:
				{
					uint32 VKCode = WParam;
					bool WasDown = ((LParam & (1 << 30)) != 0);
					//lParam & (1 << 30);
					if(VKCode == 'W')
					{
						OutputDebugStringA("W\n");
					}
					else if(VKCode == 'S')
					{
						OutputDebugStringA("W\n");
					}
					else if(VKCode == 'A')
					{
						OutputDebugStringA("W\n");
					}
					else if(VKCode == 'D')
					{
						OutputDebugStringA("W\n");
					}
					else if(VKCode == 'Q')
					{
						OutputDebugStringA("W\n");
					}
					else if(VKCode == 'E')
					{
						OutputDebugStringA("W\n");
					}
					else if(VKCode == VK_UP)
					{
						OutputDebugStringA("W\n");
					}
					else if(VKCode == VK_DOWN)
					{
						OutputDebugStringA("W\n");
					}
					else if(VKCode == VK_LEFT)
					{
						OutputDebugStringA("W\n");
					}
					else if(VKCode == VK_RIGHT)
					{
						OutputDebugStringA("W\n");
					}
					else if(VKCode == VK_ESCAPE)
					{
						OutputDebugStringA("W\n");
					}
					else if(VKCode == VK_SPACE)
					{
						OutputDebugStringA("W\n");
					}
					
				} break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
                                       GlobalBackbuffer);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
//            OutputDebugStringA("default\n");
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    
    return(Result);
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
	Win32LoadXInput();


    WNDCLASS WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
    
    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
//    WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if(RegisterClassA(&WindowClass))
    {
        HWND Window =
            CreateWindowExA(
                0,
                WindowClass.lpszClassName,
                "Handmade Hero",
                WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                Instance,
                0);
        if(Window)
        {
            // NOTE(casey): Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone.
            HDC DeviceContext = GetDC(Window);
									//int16 SampleValue = (SquareWaveCounter > (SquareWavePeriod/2)) ? 160000 : -16000;

            int XOffset = 0;
            int YOffset = 0;

						uint32 RunningSampleIndex = 0;
						int SamplesPerSecond = 48000;
						int ToneHz = 256;
						int SquareWavePeriod = SamplesPerSecond/ToneHz;
						int HalfSquareWavePeriod = SquareWavePeriod/2;
						int BytesPerSample = sizeof(int16)*2;
						int SecondaryBufferSize = SamplesPerSecond*BytesPerSample;
						int16 ToneVolume = 500;

						Win32InitDSound(Window, SamplesPerSecond, SecondaryBufferSize);
						GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;
            while(GlobalRunning)
            {
                MSG Message;

                while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }
                    
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

								// TODO: Should we poll this more frequently?
								for (DWORD ControllerIndex = 0; 
										ControllerIndex < XUSER_MAX_COUNT; 
										++ControllerIndex)
								{
									XINPUT_STATE ControllerState;
									if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
									{
										// NOTE: ERROR_SUCCESS means This controller is plugged in
										// TODO: see if ControllerState.dwPacketNumber increments too rapidly
										XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

										bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
										bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
										bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
										bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
										bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
										bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
										bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
										bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
										bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
										bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
										bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
										bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

										int16 StickX = Pad->sThumbLX;
										int16 StickY = Pad->sThumbLY;

										if (AButton)
										{
											YOffset += 2;
											// pretty sure my controller doesn't support vibration D:
								//XINPUT_VIBRATION Vibration;
								//Vibration.wLeftMotorSpeed = 60000;
								//Vibration.wRightMotorSpeed = 60000;
								//XInputSetState(0, &Vibration);
								//XInputSetState(1, &Vibration);
								//XInputSetState(2, &Vibration);
								//XInputSetState(3, &Vibration);
										}
									}
									else
									{
										// NOTE: This controller is not available
									}

								}


                RenderWeirdGradient(GlobalBackbuffer, XOffset, YOffset);

								// NOTE: DirectSound output test
								DWORD PlayCursor;
								DWORD WriteCursor;
								if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
								{
									DWORD ByteToLock = RunningSampleIndex*BytesPerSample % SecondaryBufferSize;
									DWORD BytesToWrite;
									if(ByteToLock > PlayCursor)
									{
										BytesToWrite = (SecondaryBufferSize - ByteToLock);
										BytesToWrite += PlayCursor;
									}
									else
									{
										BytesToWrite = PlayCursor - ByteToLock;
									}

									//DWORD WritePointer = ;
									//DWORD BytesToWrite = ;

									VOID *Region1;
									DWORD Region1Size;
									VOID *Region2;
									DWORD Region2Size;


									if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
																													&Region1, &Region1Size,
																													&Region2, &Region2Size,
																													0)))
									{

										// TODO: assert Region1Size/Region2Size are valid
										int16 *SampleOut = (int16 *)Region1;
										DWORD Region1SampleCount = Region1Size/BytesPerSample;
										for(DWORD SampleIndex = 0;
												SampleIndex < Region1SampleCount;
												++SampleIndex)
										{
											int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? ToneVolume : -ToneVolume;
											*SampleOut++ = SampleValue;
											*SampleOut++ = SampleValue;
										}

										DWORD Region2SampleCount = Region2Size/BytesPerSample;
										SampleOut = (int16 *)Region2;
										for(DWORD SampleIndex = 0;
												SampleIndex < Region2SampleCount;
												++SampleIndex)
										{
											int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? ToneVolume : -ToneVolume;
											*SampleOut++ = SampleValue;
											*SampleOut++ = SampleValue;
										}
									}
								}

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
                                           GlobalBackbuffer);
                
                ++XOffset;
                //YOffset += 2;
            }
        }
        else
        {
            // TODO(casey): Logging
        }
    }
    else
    {
        // TODO(casey): Logging
    }
    
    return(0);
}
