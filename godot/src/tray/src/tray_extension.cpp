#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>

#include <windows.h>
#include <shellapi.h>

using namespace godot;

// Ícone na bandeja do Windows (Shell_NotifyIconW) com menu nativo.
// Não sabe nada do jogo: emite sinais e o AppWindow decide o que fazer.
// Sobrescreve a window proc do Godot para receber os cliques da bandeja.

class TrayIcon : public Object {
	GDCLASS(TrayIcon, Object)

private:
	static TrayIcon *singleton;

	NOTIFYICONDATAW nid = {};
	bool visible = false;
	HWND hwnd = nullptr;
	HMENU menu = nullptr;
	WNDPROC prev_proc = nullptr;
	UINT wm_taskbar_created = 0;

	enum { IDM_TOGGLE = 1, IDM_STATUS = 2, IDM_QUIT = 3 };
	enum { TRAY_ID = 7, TRAY_MSG = WM_USER + 7 };

	LRESULT _handle(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
		if (msg == wm_taskbar_created && visible) {
			Shell_NotifyIconW(NIM_ADD, &nid); // explorer reiniciou: recoloca
		}
		if (msg == TRAY_MSG && (lp == WM_LBUTTONUP || lp == WM_RBUTTONUP)) {
			_popup_menu(h);
		}
		if (msg == WM_COMMAND) {
			switch (LOWORD(wp)) {
				case IDM_TOGGLE: emit_signal("toggle_pressed"); break;
				case IDM_STATUS: emit_signal("status_pressed"); break;
				case IDM_QUIT:   emit_signal("quit_pressed");   break;
			}
		}
		return CallWindowProc(prev_proc, h, msg, wp, lp);
	}

	void _popup_menu(HWND h) {
		POINT pt;
		GetCursorPos(&pt);
		SetForegroundWindow(h); // exigido para o menu fechar ao clicar fora
		TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
		PostMessage(h, WM_NULL, 0, 0);
	}

	static LRESULT CALLBACK _proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
		if (singleton != nullptr) {
			return singleton->_handle(h, msg, wp, lp);
		}
		return DefWindowProc(h, msg, wp, lp);
	}

public:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("show_icon", "icon_path", "tooltip"), &TrayIcon::show_icon);
		ClassDB::bind_method(D_METHOD("hide_icon"), &TrayIcon::hide_icon);
		ClassDB::add_signal("TrayIcon", MethodInfo("toggle_pressed"));
		ClassDB::add_signal("TrayIcon", MethodInfo("status_pressed"));
		ClassDB::add_signal("TrayIcon", MethodInfo("quit_pressed"));
	}

	TrayIcon() { singleton = this; }
	~TrayIcon() { hide_icon(); singleton = nullptr; }

	void show_icon(const String &icon_path, const String &tooltip) {
		if (visible) {
			return;
		}
		hwnd = (HWND)(intptr_t)DisplayServer::get_singleton()->window_get_window_handle(0);

		memset(&nid, 0, sizeof(nid));
		nid.cbSize           = sizeof(nid);
		nid.hWnd             = hwnd;
		nid.uID              = TRAY_ID;
		nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		nid.uCallbackMessage = TRAY_MSG;
		nid.hIcon            = (HICON)LoadImageW(nullptr, icon_path.wide_string().get_data(),
				IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
		wcsncpy_s(nid.szTip, tooltip.wide_string().get_data(), _TRUNCATE);

		if (nid.hIcon == nullptr) {
			ERR_PRINT("TrayIcon: não conseguiu carregar o ícone");
		}
		if (menu == nullptr) {
			menu = CreatePopupMenu();
			AppendMenuW(menu, MF_STRING, IDM_TOGGLE, L"Mostrar/Esconder");
			AppendMenuW(menu, MF_STRING, IDM_STATUS, L"Ver status");
			AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
			AppendMenuW(menu, MF_STRING, IDM_QUIT, L"Sair");
		}

		wm_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
		if (prev_proc == nullptr) {
			prev_proc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)_proc);
		}

		visible = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
		if (!visible) {
			ERR_PRINT("TrayIcon: Shell_NotifyIconW(NIM_ADD) falhou");
		}
	}

	void hide_icon() {
		if (visible && hwnd != nullptr) {
			Shell_NotifyIconW(NIM_DELETE, &nid);
			if (nid.hIcon != nullptr) {
				DestroyIcon(nid.hIcon);
			}
		}
		if (prev_proc != nullptr) {
			SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)prev_proc);
			prev_proc = nullptr;
		}
		if (menu != nullptr) {
			DestroyMenu(menu);
			menu = nullptr;
		}
		visible = false;
	}
};

TrayIcon *TrayIcon::singleton = nullptr;

static void initialize_tray_module(godot::ModuleInitializationLevel p_level) {
	if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	godot::ClassDB::register_class<TrayIcon>();
}

extern "C" GDExtensionBool GDE_EXPORT tamapoke_tray_library_init(
		GDExtensionInterfaceGetProcAddress p_get,
		const GDExtensionClassLibraryPtr p_lib,
		GDExtensionInitialization *r_init) {
	godot::GDExtensionBinding::InitObject init_obj(p_get, p_lib, r_init);
	init_obj.register_initializer(initialize_tray_module);
	init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
	return init_obj.init();
}
