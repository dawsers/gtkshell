#ifndef __GTKSHELL_NETWORK__
#define __GTKSHELL_NETWORK__

#include <giomm.h>
#include <gtkmm/label.h>

#include <libnm/NetworkManager.h>

class NetworkIndicator : public Gtk::Label {
public:
    NetworkIndicator() {
        network_monitor = Gio::NetworkMonitor::get_default();
        if (network_monitor->get_network_available()) {
            update_network_data();
            update_label(true);
        } else {
            update_label(false);
        }
        network_monitor->property_connectivity().signal_changed().connect(
            [this]() {
                if (network_monitor->get_network_available()) {
                    update_network_data();
                    update_label(true);
                } else {
                    update_label(false);
                }
            });
    }

private:
    void update_network_data() {
        connectivity = network_monitor->get_connectivity();
        get_network_data();
    }
    void update_label(bool available) {
        const std::vector<Glib::ustring> disabled = { "network-disabled" };
        const std::vector<Glib::ustring> enabled = { "network-enabled" };
        if (available) {
            switch (connectivity) {
                case Gio::NetworkConnectivity::FULL:
                    set_css_classes(enabled);
                    set_text(ipv4 + "  ");
                    break;
                case Gio::NetworkConnectivity::LIMITED:
                case Gio::NetworkConnectivity::LOCAL:
                case Gio::NetworkConnectivity::PORTAL:
                default:
                    set_css_classes(disabled);
                    set_text(ipv4 + "  ");
                    break;
            }
            set_tooltip_text(iface);
        } else {
            set_tooltip_text("");
            set_css_classes(disabled);
            set_text("............");
        }
    }
    void get_network_data() {
        std::string result;
        NMClient *client = nm_client_new(NULL, NULL);
        NMActiveConnection *connection = nm_client_get_primary_connection(client);
        if (connection != NULL) {
            NMIPConfig *config = nm_active_connection_get_ip4_config(connection);
            iface = nm_active_connection_get_id(connection);
            if (config != NULL) {
                GPtrArray *addresses = nm_ip_config_get_addresses(config);
                if (addresses != NULL) {
                    ipv4 = nm_ip_address_get_address((NMIPAddress *)(addresses->pdata[0]));
                }
            }
        }
    }

    Glib::RefPtr<Gio::NetworkMonitor> network_monitor;
    Gio::NetworkConnectivity connectivity;
    std::string ipv4;
    std::string iface;
};

#endif
