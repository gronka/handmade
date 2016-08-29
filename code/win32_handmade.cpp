
#include <windows.h>
#include <stdint.h>

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

// TODO: This is a global for now
global_variable bool Running;

global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable	int BytesPerPixel = 4;

internal void
RenderWeirdGradient(int XOffset, int YOffset)
{
	int Width = BitmapWidth;
	int Height = BitmapHeight;

	int Pitch = Width*BytesPerPixel;
	uint8 *Row = (uint8 *)BitmapMemory;

	for(int Y = 0;
			Y < BitmapHeight;
			++Y)
	{
		uint8 *Pixel = (uint8 *)Row;
		for(int X = 0;
				X < BitmapWidth;
				++X)
		{
			// Pixel in memory: BBGGRRxx
			*Pixel = (uint8)(X + XOffset);
			++Pixel;
			*Pixel = (uint8)(Y + YOffset);
			++Pixel;
			*Pixel = 0;
			++Pixel;
			*Pixel = 0;
			++Pixel;
		}
		Row += Pitch;
	}
}


// DIB = Device Independent Bitmap
internal void
Win32ResizeDIBSection(int Width, int Height)
{
	// TODO: bulletprrof this.
	// maybe dont free first, free after, then free first if that fails
	if(BitmapMemory)
	{
		VirtualFree(BitmapMemory, 0, MEM_RELEASE);
	}

	BitmapWidth = Width;
	BitmapHeight = Height;
	
	
	BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
	BitmapInfo.bmiHeader.biWidth = Width;
	BitmapInfo.bmiHeader.biHeight = Height;
	BitmapInfo.bmiHeader.biPlanes = 1;
	BitmapInfo.bmiHeader.biBitCount = 32;
	BitmapInfo.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (Width*Height)*BytesPerPixel;
	BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
	
	RenderWeirdGradient(0,0);

}

internal void
Win32UpdateWindow(HDC DeviceContext, RECT *WindowRect, int X, int Y, int Width, int Height)
{
	int WindowWidth = WindowRect->right - WindowRect->left;
	int WindowHeight = WindowRect->bottom - WindowRect->top;
	StretchDIBits(DeviceContext, 
			//X, Y, Width, Height,
			//X, Y, Width, Height,
			0, 0, BitmapWidth, BitmapHeight,
			0, 0, WindowWidth, WindowHeight,
			BitmapMemory,
			&BitmapInfo,
			//const BITMAPINFOR *lpBitsInfo,
			DIB_RGB_COLORS, SRCCOPY);
	
}


LRESULT CALLBACK 
Win32MainWindowCallback(HWND   Window,
							UINT   Message,
							WPARAM WParam,
							LPARAM LParam)
{
	LRESULT Result = 0;

	switch(Message)
	{
		case WM_SIZE:
		{
			RECT ClientRect;
			GetClientRect(Window, &ClientRect);
			int Width = ClientRect.right - ClientRect.left;
			int Height = ClientRect.bottom - ClientRect.top;
			Win32ResizeDIBSection(Width, Height);
			OutputDebugString("WM_SIZE\n");
		} break;

		case WM_CLOSE:
		{
			// TODO: Handle this with a message to the user?
			//PostQuitMessage(0);
			Running = false;
			//OutputDebugString("WM_CLOSE\n");
		} break;

		case WM_DESTROY:
		{
			// TODO: Handle this with an error
			Running = false;
			//OutputDebugString("WM_DESTROY\n");
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugString("WM_ACTIVATEAPP\n");
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

			RECT ClientRect;
			GetClientRect(Window, &ClientRect);

			Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
			//PatBlt(DeviceContext, X, Y, Width, Height, WHITENESS);
			EndPaint(Window, &Paint);
		} break;

		default:
		{
			//OutputDebugString("default\n");
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
	//MessageBox(0, "This is handmade hero", "Handmade Hero", 
			//MB_OK|MB_ICONINFORMATION);
	WNDCLASS WindowClass = {};

	// TODO: Check if these 3 flags even matter anymore
	WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	//WindowClass.hIcon;
	//WindowClass.lpszMenuName; topmenu for window such as "File,Edit,Help..."
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	if(RegisterClass(&WindowClass))
	{
		HWND WindowHandle =
			CreateWindowEx(
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

		if(WindowHandle)
		{
			MSG Message;
			//for(;;)
			Running = true;
			while(Running)
			{
				BOOL MessageResult = GetMessage(&Message, 0, 0, 0);
				if(MessageResult > 0)
				{
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}
				else
				{
					break;
				}
			}
		}
		else
		{
			// TODO: Logging
		}
	}
	else
	{
		// TODO: Logging
	}

	return(0);
}

