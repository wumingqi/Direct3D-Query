#include "pch.h"
#include "Application.h"

namespace Query {
	namespace Application {
		Application::Application(wstring title, HINSTANCE hInstance, UINT width, UINT height) :
			m_title(title),
			m_width(width),
			m_height(height),
			m_hInstance(hInstance)
		{
		}


#define HANDLE_MSG(msg) case msg: return app.Handle_##msg(wParam, lParam);
		LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			auto &app = *(reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)));
			switch (msg)
			{
				HANDLE_MSG(WM_PAINT);
				HANDLE_MSG(WM_SIZE);
				HANDLE_MSG(WM_KEYDOWN);
				HANDLE_MSG(WM_LBUTTONDOWN);
				HANDLE_MSG(WM_LBUTTONUP);
				HANDLE_MSG(WM_MOUSEMOVE);
				HANDLE_MSG(WM_MOUSEWHEEL);
				HANDLE_MSG(WM_DESTROY);
			default:return DefWindowProc(hWnd, msg, wParam, lParam);
			}
			return 0;
		}
#undef HANDLE_MSG

		///<summary>处理WM_PAINT消息，当客户区需要被更新时触发</summary>
		///<param name="wParam">This parameter is not used.</param>
		///<param name="lParam">This parameter is not used.</param>
		LRESULT Application::Handle_WM_PAINT(WPARAM wParam, LPARAM lParam)
		{
			query.OnUpdate();
			query.OnRender();
			return 0;
		}

		///<summary>处理WM_LBUTTONDOWN消息，鼠标左键按下时触发</summary>
		///<param name="wParam">表示是否有虚拟按键按下，如MK_CONTROL等</param>
		///<param name="lParam">相对于客户区左上角的光标的坐标</param>
		LRESULT Application::Handle_WM_LBUTTONDOWN(WPARAM wParam, LPARAM lParam)
		{
			return 0;
		}

		///<summary>处理WM_LBUTTONUP消息，鼠标左键松开时触发</summary>
		///<param name="wParam">表示是否有虚拟按键按下，如MK_CONTROL等</param>
		///<param name="lParam">相对于客户区左上角的光标的坐标</param>
		LRESULT Application::Handle_WM_LBUTTONUP(WPARAM wParam, LPARAM lParam)
		{
			return 0;
		}

		///<summary>处理WM_MOUSE消息，鼠标在客户区移动时触发</summary>
		///<param name="wParam">表示是否有虚拟按键按下，如MK_CONTROL等</param>
		///<param name="lParam">相对于客户区左上角的光标的坐标</param>
		LRESULT Application::Handle_WM_MOUSEMOVE(WPARAM wParam, LPARAM lParam)
		{
			return 0;
		}

		///<summary>处理WM_SIZE消息，当窗口大小改变时触发</summary>
		///<param name="wParam">表示大小改变的类型，如SIZE_MAXIMIZED、SIZE_HIDE等</param>
		///<param name="lParam">窗口客户区的大小</param>
		LRESULT Application::Handle_WM_SIZE(WPARAM wParam, LPARAM lParam)
		{
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);
			m_width = width; m_height = height;
			return 0;
		}

		///<summary>处理WM_KEYDOWN消息，当键盘按键按下时触发</summary>
		///<param name="wParam">非系统键的虚拟键值（Virtual-Key Code）</param>
		///<param name="lParam">其他信息</param>
		LRESULT Application::Handle_WM_KEYDOWN(WPARAM wParam, LPARAM lParam)
		{

			return 0;
		}

		///<summary>处理WM_MOUSEWHEEL消息，当鼠标滚轮滚动时触发</summary>
		///<param name="wParam">
		///高字表明滚轮滚动的方向，正值表明向前滚，负值表明向后滚
		///低字表明时候有其他虚拟键按下（MK_CONTROL、等）
		///</param>
		///<param name="lParam">鼠标的坐标位置，相对于屏幕左上角</param>
		LRESULT Application::Handle_WM_MOUSEWHEEL(WPARAM wParam, LPARAM lParam)
		{
			auto flag = HIWORD(wParam) & 0x8000;
			if (flag)
			{

			}
			else
			{

			}
			return 0;
		}
		
		LRESULT Application::Handle_WM_DESTROY(WPARAM wParam, LPARAM lParam)
		{
			query.OnDestroy();
			PostQuitMessage(0);
			return 0;
		}
	}
}
