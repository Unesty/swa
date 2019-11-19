#include <swa/winapi.h>
#include <dlg/dlg.h>

#ifdef SWA_WITH_VK
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#endif

static const struct swa_display_interface display_impl;
static const struct swa_window_interface window_impl;

// utility
static struct swa_display_win* get_display_win(struct swa_display* base) {
	dlg_assert(base->impl == &display_impl);
	return (struct swa_display_win*) base;
}

static struct swa_window_win* get_window_win(struct swa_window* base) {
	dlg_assert(base->impl == &window_impl);
	return (struct swa_window_win*) base;
}

// defined as macro so we don't lose file and line information
#define print_winapi_error(func) do { \
	wchar_t* buffer; \
	int code = GetLastError(); \
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | \
		FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, \
		code, 0, (wchar_t*) &buffer, 0, NULL); \
	dlg_errort(("winapi_error"), "%s: %ls", func, buffer); \
	LocalFree(buffer); \
} while(0)

// window api
static void win_destroy(struct swa_window* base) {
	struct swa_window_win* win = get_window_win(base);
    free(win);
}

static enum swa_window_cap win_get_capabilities(struct swa_window* base) {
    (void) base;
    return swa_window_cap_cursor |
        swa_window_cap_fullscreen |
        swa_window_cap_maximize |
        swa_window_cap_minimize |
        swa_window_cap_size |
        swa_window_cap_position |
        swa_window_cap_size_limits |
        swa_window_cap_title |
        swa_window_cap_visibility;
}

static void win_set_min_size(struct swa_window* base, unsigned w, unsigned h) {
	struct swa_window_win* win = get_window_win(base);
}

static void win_set_max_size(struct swa_window* base, unsigned w, unsigned h) {
	struct swa_window_win* win = get_window_win(base);
}

static void win_show(struct swa_window* base, bool show) {
	struct swa_window_win* win = get_window_win(base);
}

static void win_set_size(struct swa_window* base, unsigned w, unsigned h) {
	struct swa_window_win* win = get_window_win(base);
}

static void win_set_position(struct swa_window* base, int x, int y) {
	struct swa_window_win* win = get_window_win(base);
}

static void win_set_cursor(struct swa_window* base, struct swa_cursor cursor) {
	struct swa_window_win* win = get_window_win(base);
}

static void win_refresh(struct swa_window* base) {
	struct swa_window_win* win = get_window_win(base);
}

static void win_surface_frame(struct swa_window* base) {
    (void) base;
    // no-op
}

static void win_set_state(struct swa_window* base, enum swa_window_state state) {
	struct swa_window_win* win = get_window_win(base);
}

static void win_begin_move(struct swa_window* base) {
}

static void win_begin_resize(struct swa_window* base, enum swa_edge edges) {
}

static void win_set_title(struct swa_window* base, const char* title) {
}

static void win_set_icon(struct swa_window* base, const struct swa_image* img) {
}

static bool win_is_client_decorated(struct swa_window* base) {
	return false;
}

static uint64_t win_get_vk_surface(struct swa_window* base) {
	return false;
}

static bool win_gl_make_current(struct swa_window* base) {
	return false;
}

static bool win_gl_swap_buffers(struct swa_window* base) {
	return false;
}

static bool win_gl_set_swap_interval(struct swa_window* base, int interval) {
	return false;
}

static bool win_get_buffer(struct swa_window* base, struct swa_image* img) {
	struct swa_window_win* win = get_window_win(base);
	if(win->surface_type != swa_surface_buffer) {
		dlg_error("Window doesn't have buffer surface");
		return false;
	}

	if(win->buffer.active) {
		dlg_error("There is already an active buffer");
		return false;
	}

	win->buffer.wdc = GetDC(win->handle);
	if(!win->buffer.wdc) {
		print_winapi_error("GetDC");
		return false;
	}

	// check if we have to recreate the bitmap
	if(win->buffer.width != win->width || win->buffer.height != win->height) {
		if(win->buffer.bitmap) {
			DeleteObject(win->buffer.bitmap);
		}

		win->buffer.width = win->width;
		win->buffer.height = win->height;

		BITMAPINFO bmi = {0};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = win->buffer.width;
		bmi.bmiHeader.biHeight = -win->buffer.height; // top down
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		win->buffer.bitmap = CreateDIBSection(win->buffer.wdc, &bmi, DIB_RGB_COLORS,
			&win->buffer.data, NULL, 0);
		if(!win->buffer.bitmap) {
			print_winapi_error("CreateDIBSection");
			return false;
		}
	}

	win->buffer.active = true;

	// See documentation for BITMAPINFOHEADER.
	// The bitmap RGB format specifies that blue is in the least
	// significant 8 bits and unused (alpha) the 8 most significant
	// bits. We therefore have word-order argb32
	img->format = swa_image_format_toggle_byte_word(swa_image_format_argb32);
	img->data = win->buffer.data;
	img->stride = 4 * win->width;
	img->width = win->width;
	img->height = win->height;

	return true;
}

static void win_apply_buffer(struct swa_window* base) {
	struct swa_window_win* win = get_window_win(base);
	if(win->surface_type != swa_surface_buffer) {
		dlg_error("Window doesn't have buffer surface");
		return;
	}

	if(!win->buffer.active) {
		dlg_error("There is no active buffer to apply");
		return;
	}

	dlg_assert(win->buffer.bitmap);
	dlg_assert(win->buffer.wdc);

	HDC bdc = CreateCompatibleDC(win->buffer.wdc);
	if(!bdc) {
		print_winapi_error("CreateCompatibleDC");
		goto cleanup;
	}

	HGDIOBJ prev = SelectObject(bdc, win->buffer.bitmap);
	if(!prev) {
		print_winapi_error("SelectObject");
		goto cleanup_bdc;
	}

	bool res = BitBlt(win->buffer.wdc, 0, 0,
		win->buffer.width, win->buffer.height, bdc, 0, 0, SRCCOPY);
	if(!res) {
		print_winapi_error("BitBlt");
	}
	
	SelectObject(bdc, prev);

cleanup_bdc:
	DeleteDC(bdc);

cleanup:
	ReleaseDC(win->handle, win->buffer.wdc);
	win->buffer.active = false;
	win->buffer.wdc = NULL;
}

static const struct swa_window_interface window_impl = {
	.destroy = win_destroy,
	.get_capabilities = win_get_capabilities,
	.set_min_size = win_set_min_size,
	.set_max_size = win_set_max_size,
	.show = win_show,
	.set_size = win_set_size,
	.set_position = win_set_position,
	.refresh = win_refresh,
	.surface_frame = win_surface_frame,
	.set_state = win_set_state,
	.begin_move = win_begin_move,
	.begin_resize = win_begin_resize,
	.set_title = win_set_title,
	.set_icon = win_set_icon,
	.is_client_decorated = win_is_client_decorated,
	.get_vk_surface = win_get_vk_surface,
	.gl_make_current = win_gl_make_current,
	.gl_swap_buffers = win_gl_swap_buffers,
	.gl_set_swap_interval = win_gl_set_swap_interval,
	.get_buffer = win_get_buffer,
	.apply_buffer = win_apply_buffer
};


// display api
void display_destroy(struct swa_display* base) {
	struct swa_display_win* dpy = get_display_win(base);
    free(dpy);
}

static bool dispatch_one(void) {
	MSG msg;
	if(!PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
		return false;
	}

	TranslateMessage(&msg);
	DispatchMessage(&msg);
	return true;
}

bool display_dispatch(struct swa_display* base, bool block) {
	struct swa_display_win* dpy = get_display_win(base);
    if(dpy->error) {
        return false;
    }

    // wait for first message if we are allowed to block
    if(block) {
        MSG msg;
        int ret = GetMessage(&msg, NULL, 0, 0);
        if(ret == -1) {
            // winapi documentation suggests that errors here are
            // of critical nature and exiting the application is the
            // usual strategy.
			print_winapi_error("Critical error in GetMessage");
            dpy->error = true;
            return false;
        } else {
		    TranslateMessage(&msg);
		    DispatchMessage(&msg);
        }
    }

    // dispatch all messages
    while(dispatch_one());
    return true;
}

void display_wakeup(struct swa_display* base) {
	struct swa_display_win* dpy = get_display_win(base);
}

enum swa_display_cap display_capabilities(struct swa_display* base) {
	struct swa_display_win* dpy = get_display_win(base);
	enum swa_display_cap caps =
#ifdef SWA_WITH_GL
		swa_display_cap_gl |
#endif
#ifdef SWA_WITH_VK
		swa_display_cap_vk |
#endif
		swa_display_cap_client_decoration |
		swa_display_cap_keyboard |
		swa_display_cap_mouse |
		swa_display_cap_touch |
        // TODO: implement data exchange
		// swa_display_cap_dnd |
		// swa_display_cap_clipboard |
		swa_display_cap_buffer_surface;
	return caps;
}

const char** display_vk_extensions(struct swa_display* base, unsigned* count) {
#ifdef SWA_WITH_VK
	static const char* names[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
	};
	*count = sizeof(names) / sizeof(names[0]);
	return names;
#else
	dlg_warn("swa was compiled without vulkan suport");
	*count = 0;
	return NULL;
#endif
}

bool display_key_pressed(struct swa_display* base, enum swa_key key) {
	struct swa_display_win* dpy = get_display_win(base);
    return false;
}

const char* display_key_name(struct swa_display* base, enum swa_key key) {
	struct swa_display_win* dpy = get_display_win(base);
    return NULL;
}

enum swa_keyboard_mod display_active_keyboard_mods(struct swa_display* base) {
	struct swa_display_win* dpy = get_display_win(base);
    return swa_keyboard_mod_none;
}

struct swa_window* display_get_keyboard_focus(struct swa_display* base) {
	struct swa_display_win* dpy = get_display_win(base);
    return NULL;
}

bool display_mouse_button_pressed(struct swa_display* base, enum swa_mouse_button button) {
	struct swa_display_win* dpy = get_display_win(base);
    return false;
}
void display_mouse_position(struct swa_display* base, int* x, int* y) {
	struct swa_display_win* dpy = get_display_win(base);
}
struct swa_window* display_get_mouse_over(struct swa_display* base) {
	struct swa_display_win* dpy = get_display_win(base);
    return NULL;
}
struct swa_data_offer* display_get_clipboard(struct swa_display* base) {
	// struct swa_display_win* dpy = get_display_win(base);
    return NULL;
}
bool display_set_clipboard(struct swa_display* base,
		struct swa_data_source* source) {
	// struct swa_display_win* dpy = get_display_win(base);
	return false;
}
bool display_start_dnd(struct swa_display* base,
		struct swa_data_source* source) {
	// struct swa_display_win* dpy = get_display_win(base);
	return false;
}

static LRESULT CALLBACK win_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	struct swa_window_win* win = (struct swa_window_win*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if(!win) {
    	return DefWindowProc(hwnd, msg, wparam, lparam); 
	}

	LRESULT result = 0;
	switch(msg) {
		case WM_PAINT: {
			if(win->width == 0 && win->height == 0) {
				break;
			}

			if(win->base.listener->draw) {
				win->base.listener->draw(&win->base);
			}
			result = DefWindowProc(hwnd, msg, wparam, lparam); // to validate the window
			break;
		} case WM_DESTROY: {
			if(win->base.listener->close) {
				win->base.listener->close(&win->base);
			}
			break;
		} case WM_SIZE: {
			unsigned width = LOWORD(lparam);
			unsigned height = HIWORD(lparam);
			if((win->width == width && win->height == height) || 
					(width == 0 && height == 0)) {
				break;
			}

			win->width = width;
			win->height = height;
			if(win->base.listener->resize) {
				win->base.listener->resize(&win->base, width, height);
			}
			break;
		} case WM_SYSCOMMAND: {
			enum swa_window_state state = swa_window_state_none;

			if(wparam == SC_MAXIMIZE) {
				state = swa_window_state_maximized;
			} else if(wparam == SC_MINIMIZE) {
				state = swa_window_state_minimized;
			} else if(wparam == SC_RESTORE) {
				state = swa_window_state_normal;
			} else if(wparam >= SC_SIZE && wparam <= SC_SIZE + 8) {
			// 	// TODO: set cursor
			// 	auto currentCursor = GetClassLongPtr(handle(), -12);
			// 	auto edge = winapiToEdges(wparam - SC_SIZE);
			// 	auto c = sizeCursorFromEdge(edge);
			// 	cursor(c);
			// 	result = ::DefWindowProc(handle(), message, wparam, lparam);
			// 	::SetClassLongPtr(handle(), -12, currentCursor);
			// 	break;
			}

			if(state != swa_window_state_none && win->base.listener->state) {
				win->base.listener->state(&win->base, state);
			}

			result = DefWindowProc(hwnd, msg, wparam, lparam);
			break;
		} case WM_GETMINMAXINFO: {
			MINMAXINFO* mmi = (MINMAXINFO*)(lparam);
			mmi->ptMaxTrackSize.x = win->max_width;
			mmi->ptMaxTrackSize.y = win->max_height;
			mmi->ptMinTrackSize.x = win->min_width;
			mmi->ptMinTrackSize.y = win->min_height;
			break;
		} case WM_ERASEBKGND: {
			result = 1; // prevent the background erase
			break;
		} default: {
    		return DefWindowProc(hwnd, msg, wparam, lparam); 
		}
	}

	return result;
}

static wchar_t* widen(const char* utf8) {
	int count = MultiByteToWideChar(CP_UTF8, 0 , utf8, -1, NULL, 0);
	wchar_t* wide = malloc(count * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, count);
	return wide;
}

static char* narrow(const wchar_t* wide) {
	int count = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
	char* utf8 = malloc(count * sizeof(char));
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, count, NULL, NULL);
	return utf8;
}

struct swa_window* display_create_window(struct swa_display* base,
		const struct swa_window_settings* settings) {
	struct swa_display_win* dpy = get_display_win(base);
	struct swa_window_win* win = calloc(1, sizeof(*win));

	win->base.impl = &window_impl;
	win->base.listener = settings->listener;
	win->dpy = dpy;

	// TODO, use GetSystemMetrics. See docs for MINMAXSIZE
	win->max_width = win->max_height = 9999;
	HINSTANCE hinstance = GetModuleHandle(NULL);

	// new window class
	WNDCLASSEX wcx;
	wcx.cbSize = sizeof(wcx);
	wcx.style = CS_HREDRAW | CS_VREDRAW;
	wcx.lpfnWndProc = win_proc;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = hinstance;
	wcx.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcx.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcx.hbrBackground = NULL;
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = L"MainWClass";
	
	if(!RegisterClassExW(&wcx)) {
		print_winapi_error("RegisterClassEx");
		goto error;
	}

	// create window
	unsigned exstyle = 0;
	unsigned style = WS_OVERLAPPEDWINDOW;
	if(settings->transparent) {
		exstyle |= WS_EX_LAYERED;
		style |= CS_OWNDC;
	}

	wchar_t* titlew = settings->title ? widen(settings->title) : L"";
	int width = settings->width == SWA_DEFAULT_SIZE ? CW_USEDEFAULT : settings->width;
	int height = settings->height == SWA_DEFAULT_SIZE ? CW_USEDEFAULT : settings->height;
	win->handle = CreateWindowEx(
		exstyle,
		L"MainWClass",
		titlew,
		style,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height,
		NULL, NULL, hinstance, win);
	if(!win->handle) {
		print_winapi_error("CreateWindowEx");
		goto error;
	}

	SetWindowLongPtr(win->handle, GWLP_USERDATA, (uintptr_t) win);
	ShowWindowAsync(win->handle, SW_SHOWDEFAULT);

	// surface
	win->surface_type = settings->surface;
	// no-op for buffer surface

	return &win->base;

error:
	win_destroy(&win->base);
	return NULL;
}

static const struct swa_display_interface display_impl = {
	.destroy = display_destroy,
	.dispatch = display_dispatch,
	.wakeup = display_wakeup,
	.capabilities = display_capabilities,
	.vk_extensions = display_vk_extensions,
	.key_pressed = display_key_pressed,
	.key_name = display_key_name,
	.active_keyboard_mods = display_active_keyboard_mods,
	.get_keyboard_focus = display_get_keyboard_focus,
	.mouse_button_pressed = display_mouse_button_pressed,
	.mouse_position = display_mouse_position,
	.get_mouse_over = display_get_mouse_over,
	.get_clipboard = display_get_clipboard,
	.set_clipboard = display_set_clipboard,
	.start_dnd = display_start_dnd,
	.create_window = display_create_window,
};

struct swa_display* swa_display_win_create(void) {
    struct swa_display_win* dpy = calloc(1, sizeof(*dpy));
    dpy->base.impl = &display_impl;
    return &dpy->base;
}