#include "stdafx.h"

#include <SDK/output.h>
#include <SDK/coreDarkMode.h>

#include <libPPUI/win32_op.h>
#include <helpers/BumpableElem.h>

#include <vector>

// A Default UI element that shows the output device foobar2000 is currently
// using, and lets the user pick a different one from a drop-down - no
// Preferences window involved.
//
// Reading and writing the setting goes through output_manager_v2, the same API
// the Preferences page uses, so anything the core supports (WASAPI, ASIO, DS,
// UPnP, ...) shows up here without special casing.

namespace {

// Substitute your own GUID when reusing this code.
const GUID guid_output_switcher_elem = { 0x2b7f4c19, 0x5d3a, 0x4e88, { 0x9c, 0x41, 0x6e, 0x2f, 0xb5, 0x07, 0xd8, 0x21 } };

// Posted (not sent) from output_config_change_callback, which can fire on any
// thread, so the work always happens on the window's thread.
const UINT WM_OSW_REFRESH = WM_APP + 71;

const int IDC_OSW_DEVICE = 1001;

struct DeviceEntry {
    pfc::string8 name;
    GUID output_guid;
    GUID device_guid;
};

class OutputSwitcherElem : public ui_element_instance,
                           public CWindowImpl<OutputSwitcherElem>,
                           private output_config_change_callback {
public:
    DECLARE_WND_CLASS_EX(TEXT("{2B7F4C19-5D3A-4E88-9C41-6E2FB507D821}"), CS_VREDRAW | CS_HREDRAW, (-1));

    OutputSwitcherElem(ui_element_config::ptr config, ui_element_instance_callback_ptr callback)
        : m_config(config), m_callback(callback) {}

    ~OutputSwitcherElem() { detach_output_api(); }

    void initialize_window(HWND parent) { WIN32_OP(Create(parent) != NULL); }

    BEGIN_MSG_MAP_EX(OutputSwitcherElem)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestroy)
        MSG_WM_SIZE(OnSize)
        MSG_WM_PAINT(OnPaint)
        MSG_WM_ERASEBKGND(OnEraseBkgnd)
        MSG_WM_RBUTTONUP(OnRButtonUp)
        MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
        MESSAGE_HANDLER(WM_COMMAND, OnCommand)
        MESSAGE_HANDLER(WM_OSW_REFRESH, OnRefreshMessage)
    END_MSG_MAP()

    HWND get_wnd() { return *this; }
    void set_configuration(ui_element_config::ptr config) { m_config = config; }
    ui_element_config::ptr get_configuration() { return m_config; }

    static GUID g_get_guid() { return guid_output_switcher_elem; }
    static GUID g_get_subclass() { return ui_element_subclass_utility; }
    static void g_get_name(pfc::string_base & out) { out = "Output device"; }
    static ui_element_config::ptr g_get_default_configuration() { return ui_element_config::g_create_empty(g_get_guid()); }
    static const char * g_get_description() {
        return "Shows the current output device and lets you switch it without opening Preferences.";
    }

    void notify(const GUID & what, t_size param1, const void * param2, t_size param2_size);

private:
    int OnCreate(LPCREATESTRUCT create_struct);
    void OnDestroy() { detach_output_api(); }
    void OnSize(UINT type, CSize size) { layout_children(); }
    void OnPaint(CDCHandle dc);
    BOOL OnEraseBkgnd(CDCHandle dc);
    void OnRButtonUp(UINT flags, CPoint point);
    void OnGetMinMaxInfo(LPMINMAXINFO info);
    LRESULT OnCommand(UINT, WPARAM wparam, LPARAM, BOOL & handled);
    LRESULT OnRefreshMessage(UINT, WPARAM, LPARAM, BOOL & handled) {
        reload_devices();
        handled = TRUE;
        return 0;
    }

    // output_config_change_callback: somebody (us, Preferences, another
    // component) changed the output configuration.
    void outputConfigChanged() override { PostMessage(WM_OSW_REFRESH); }

    void attach_output_api();
    void detach_output_api();
    void reload_devices();
    void populate_combo();
    void select_current_device();
    void apply_selection(int index);
    void layout_children();
    int closed_height() const;

    CComboBox m_combo;
    std::vector<DeviceEntry> m_devices;
    ui_element_config::ptr m_config;
    bool m_api_attached = false;
    // Set while the drop-down is being rebuilt, so the resulting selection
    // change is not mistaken for the user picking a device.
    bool m_populating = false;

protected:
    // Must stay protected: ui_element_impl_withpopup needs to reach it.
    const ui_element_instance_callback_ptr m_callback;
    fb2k::CCoreDarkModeHooks m_dark;
};

int OutputSwitcherElem::OnCreate(LPCREATESTRUCT) {
    // Created far taller than it looks on purpose. A CBS_DROPDOWNLIST combo uses
    // its own window height as the height of the list that opens on click, and
    // the part that sticks out of our client area is simply clipped away.
    CRect rc(0, 0, 240, 400);
    if (m_combo.Create(m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_HASSTRINGS, 0,
                       IDC_OSW_DEVICE) == NULL)
        return -1;

    m_combo.SetFont(m_callback->query_font_ex(ui_font_default));
    m_dark.AddDialogWithControls(m_hWnd);

    attach_output_api();
    reload_devices();
    return 0;
}

void OutputSwitcherElem::OnGetMinMaxInfo(LPMINMAXINFO info) {
    // Small enough for a toolbar, wide enough to read a device name.
    info->ptMinTrackSize.x = 140;
    info->ptMinTrackSize.y = closed_height() + 4;
}

void OutputSwitcherElem::attach_output_api() {
    if (m_api_attached) return;
    output_manager_v2::ptr api;
    if (output_manager_v2::tryGet(api)) {
        api->addCallback(this);
        m_api_attached = true;
    }
}

void OutputSwitcherElem::detach_output_api() {
    if (!m_api_attached) return;
    output_manager_v2::ptr api;
    if (output_manager_v2::tryGet(api)) api->removeCallback(this);
    m_api_attached = false;
}

void OutputSwitcherElem::reload_devices() {
    m_devices.clear();

    output_manager_v2::ptr api;
    if (output_manager_v2::tryGet(api)) {
        // Enumerating devices talks to every installed output module, and an
        // ASIO driver in a bad mood can throw; the element must survive that.
        try {
            api->listDevices([this](const char * full_name, const GUID & output_guid, const GUID & device_guid) {
                DeviceEntry entry;
                entry.name = full_name;
                entry.output_guid = output_guid;
                entry.device_guid = device_guid;
                m_devices.push_back(entry);
            });
        } catch (std::exception const & e) {
            FB2K_console_formatter() << "Output device switcher: could not enumerate devices: " << e.what();
        }
    }

    populate_combo();
    select_current_device();
    layout_children();
    Invalidate();
}

void OutputSwitcherElem::populate_combo() {
    if (!m_combo) return;
    m_populating = true;
    m_combo.ResetContent();
    for (auto & device : m_devices) {
        pfc::stringcvt::string_wide_from_utf8 wide_name(device.name);
        m_combo.AddString(wide_name);
    }
    m_combo.SetCurSel(-1);
    m_populating = false;
}

void OutputSwitcherElem::select_current_device() {
    if (!m_combo) return;

    int selection = -1;
    output_manager_v2::ptr api;
    if (output_manager_v2::tryGet(api)) {
        const outputCoreConfig_t config = api->getCoreConfig();
        for (size_t i = 0; i < m_devices.size(); i++) {
            if (pfc::guid_equal(m_devices[i].output_guid, config.m_output) &&
                pfc::guid_equal(m_devices[i].device_guid, config.m_device)) {
                selection = (int)i;
                break;
            }
        }
    }

    m_populating = true;
    m_combo.SetCurSel(selection);
    m_populating = false;
}

void OutputSwitcherElem::apply_selection(int index) {
    if (index < 0 || (size_t)index >= m_devices.size()) return;

    output_manager_v2::ptr api;
    if (!output_manager_v2::tryGet(api)) return;

    try {
        // The core restarts the output when the device changes, exactly as it
        // does from the Preferences page.
        api->setCoreConfigDevice(m_devices[index].output_guid, m_devices[index].device_guid);
    } catch (std::exception const & e) {
        FB2K_console_formatter() << "Output device switcher: could not switch device: " << e.what();
    }
}

int OutputSwitcherElem::closed_height() const {
    // The closed part of a drop-down list combo is its font height plus the
    // borders it draws around it.
    if (m_combo) {
        const int item_height = m_combo.GetItemHeight(-1);
        if (item_height > 0) return item_height + 6;
    }
    return 22;
}

void OutputSwitcherElem::layout_children() {
    if (!m_combo) return;

    CRect rc;
    if (!GetClientRect(&rc)) return;

    if (m_devices.empty()) {
        m_combo.ShowWindow(SW_HIDE);
        return;
    }
    m_combo.ShowWindow(SW_SHOW);

    const int height = closed_height();
    const int y = (rc.Height() - height) / 2;
    int width = rc.Width() - 4;
    if (width < 40) width = 40;

    m_combo.MoveWindow(2, y < 0 ? 0 : y, width, 400);
}

LRESULT OutputSwitcherElem::OnCommand(UINT, WPARAM wparam, LPARAM, BOOL & handled) {
    if (LOWORD(wparam) != IDC_OSW_DEVICE) return 0;

    switch (HIWORD(wparam)) {
    case CBN_SELCHANGE:
        handled = TRUE;
        if (!m_populating) apply_selection(m_combo.GetCurSel());
        break;
    case CBN_DROPDOWN:
        // Devices appear and disappear while foobar2000 runs (USB DAC unplugged,
        // ASIO driver restarted), so the list is rebuilt every time it opens.
        handled = TRUE;
        reload_devices();
        break;
    default:
        break;
    }
    return 0;
}

void OutputSwitcherElem::OnRButtonUp(UINT, CPoint point) {
    CMenu menu;
    if (!menu.CreatePopupMenu()) return;
    menu.AppendMenu(MF_STRING, 1, L"Refresh device list");
    CPoint screen_point(point);
    ClientToScreen(&screen_point);
    const UINT command = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_RIGHTBUTTON, screen_point.x, screen_point.y, m_hWnd);
    if (command == 1) reload_devices();
}

BOOL OutputSwitcherElem::OnEraseBkgnd(CDCHandle dc) {
    CRect rc;
    if (!GetClientRect(&rc)) return FALSE;
    CBrush brush;
    if (brush.CreateSolidBrush(m_callback->query_std_color(ui_color_background)) == NULL) return FALSE;
    dc.FillRect(&rc, brush);
    return TRUE;
}

void OutputSwitcherElem::OnPaint(CDCHandle) {
    CPaintDC dc(*this);
    if (!m_devices.empty()) return; // the combo box covers the whole client area

    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(m_callback->query_std_color(ui_color_text));
    SelectObjectScope font_scope(dc, (HGDIOBJ)m_callback->query_font_ex(ui_font_default));

    CRect rc;
    if (!GetClientRect(&rc)) return;
    dc.DrawText(L"No output device", -1, &rc, DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void OutputSwitcherElem::notify(const GUID & what, t_size, const void *, t_size) {
    if (what == ui_element_notify_font_changed) {
        if (m_combo) m_combo.SetFont(m_callback->query_font_ex(ui_font_default));
        layout_children();
        Invalidate();
    } else if (what == ui_element_notify_colors_changed) {
        Invalidate();
    }
}

// ui_element_impl_withpopup also gives us a standalone window and a "Utilities"
// menu entry for free; the element itself is what the user docks into a toolbar.
class OutputSwitcherElemImpl : public ui_element_impl_withpopup<OutputSwitcherElem> {};

static service_factory_single_t<OutputSwitcherElemImpl> g_output_switcher_elem_factory;

} // namespace
