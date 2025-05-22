#ifndef __GTKSHELL_DBUS__
#define __GTKSHELL_DBUS__

#include <glibmm.h>

const Glib::ustring DBUS_MPRIS_MEDIAPLAYER2 = R"""(
<node>
    <interface name="org.mpris.MediaPlayer2">
        <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
        <method name="Raise" />
        <method name="Quit" />
        <property name="CanQuit" type="b" access="read" />
        <property name="Fullscreen" type="b" access="readwrite" />
        <property name="CanSetFullscreen" type="b" access="read" />
        <property name="CanRaise" type="b" access="read" />
        <property name="HasTrackList" type="b" access="read" />
        <property name="Identity" type="s" access="read" />
        <property name="DesktopEntry" type="s" access="read">
            <annotation name="org.mpris.MediaPlayer2.property.optional" value="true" />
        </property>
        <property name="SupportedUriSchemes" type="as" access="read" />
        <property name="SupportedMimeTypes" type="as" access="read" />
    </interface>
</node>
)""";

const Glib::ustring DBUS_MPRIS_MEDIAPLAYER2_PLAYER = R"""(
<node>
    <interface name="org.mpris.MediaPlayer2.Player">
        <method name="Next" />
        <method name="Previous" />
        <method name="Pause" />
        <method name="PlayPause" />
        <method name="Stop" />
        <method name="Play" />
        <method name="Seek">
            <arg direction="in" type="x" name="Offset" />
        </method>
        <method name="SetPosition">
            <arg direction="in" type="o" name="TrackId" />
            <arg direction="in" type="x" name="Position" />
        </method>
        <method name="OpenUri">
            <arg direction="in" type="s" name="Uri" />
        </method>
        <property name="PlaybackStatus" type="s" access="read">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
        </property>
        <property name="LoopStatus" type="s" access="readwrite">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
            <annotation name="org.mpris.MediaPlayer2.property.optional" value="true" />
        </property>
        <property name="Rate" type="d" access="readwrite">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
        </property>
        <property name="Shuffle" type="b" access="readwrite">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
            <annotation name="org.mpris.MediaPlayer2.property.optional" value="true" />
        </property>
        <property name="Metadata" type="a{sv}" access="read">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
        </property>
        <property name="Volume" type="d" access="readwrite">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
        </property>
        <property name="Position" type="x" access="read">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="false" />
        </property>
        <property name="MinimumRate" type="d" access="read">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
        </property>
        <property name="MaximumRate" type="d" access="read">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
        </property>
        <property name="CanGoNext" type="b" access="read">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
</property>
        <property name="CanGoPrevious" type="b" access="read">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
        </property>
        <property name="CanPlay" type="b" access="read">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
        </property>
        <property name="CanPause" type="b" access="read">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
        </property>
        <property name="CanSeek" type="b" access="read">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true" />
        </property>
        <property name="CanControl" type="b" access="read">
            <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="false" />
        </property>
        <signal name="Seeked">
            <arg name="Position" type="x" />
        </signal>
    </interface>
</node>
)""";

const Glib::ustring DBUS_CANONICAL_DBUSMENU = R"""(
<node>
    <interface name="com.canonical.dbusmenu">
        <property name="Version" type="u" access="read"/>
        <property name="TextDirection" type="s" access="read"/>
        <property name="Status" type="s" access="read"/>
        <property name="IconThemePath" type="as" access="read"/>
        <method name="GetLayout">
            <arg type="i" name="parentId" direction="in"/>
            <arg type="i" name="recursionDepth" direction="in"/>
            <arg type="as" name="propertyNames" direction="in"/>
            <arg type="u" name="revision" direction="out"/>
            <arg type="(ia{sv}av)" name="layout" direction="out"/>
        </method>
        <method name="GetGroupProperties">
            <arg type="ai" name="ids" direction="in"/>
            <arg type="as" name="propertyNames" direction="in"/>
            <arg type="a(ia{sv})" name="properties" direction="out"/>
        </method>
        <method name="GetProperty">
            <arg type="i" name="id" direction="in"/>
            <arg type="s" name="name" direction="in"/>
            <arg type="v" name="value" direction="out"/>
        </method>
        <method name="Event">
            <arg type="i" name="id" direction="in"/>
            <arg type="s" name="eventId" direction="in"/>
            <arg type="v" name="data" direction="in"/>
            <arg type="u" name="timestamp" direction="in"/>
        </method>
        <method name="EventGroup">
            <arg type="a(isvu)" name="events" direction="in"/>
            <arg type="ai" name="idErrors" direction="out"/>
        </method>
        <method name="AboutToShow">
            <arg type="i" name="id" direction="in"/>
            <arg type="b" name="needUpdate" direction="out"/>
        </method>
        <method name="AboutToShowGroup">
            <arg type="ai" name="ids" direction="in"/>
            <arg type="ai" name="updatesNeeded" direction="out"/>
            <arg type="ai" name="idErrors" direction="out"/>
        </method>
        <signal name="ItemsPropertiesUpdated">
            <arg type="a(ia{sv})" name="updatedProps" direction="out" />
            <arg type="a(ias)" name="removedProps" direction="out" />
        </signal>
        <signal name="LayoutUpdated">
            <arg type="u" name="revision" direction="out"/>
            <arg type="i" name="parent" direction="out"/>
        </signal>
        <signal name="ItemActivationRequested">
            <arg type="i" name="id" direction="out"/>
            <arg type="u" name="timestamp" direction="out"/>
        </signal>
    </interface>
</node>
)""";

const Glib::ustring DBUS_FREEDESKTOP_NOTIFICATIONS = R"""(
<node>
    <interface name="org.freedesktop.Notifications">
        <method name="Notify">
            <arg type="s" direction="in"/>
            <arg type="u" direction="in"/>
            <arg type="s" direction="in"/>
            <arg type="s" direction="in"/>
            <arg type="s" direction="in"/>
            <arg type="as" direction="in"/>
            <arg type="a{sv}" direction="in"/>
            <arg type="i" direction="in"/>
            <arg type="u" direction="out"/>
        </method>
        <method name="CloseNotification">
            <arg type="u" direction="in" name="id"/>
        </method>
        <method name="GetCapabilities">
            <arg type="as" direction="out"/>
        </method>
        <method name="GetServerInformation">
            <arg type="s" direction="out"/>
            <arg type="s" direction="out"/>
            <arg type="s" direction="out"/>
            <arg type="s" direction="out"/>
        </method>
        <signal name="NotificationClosed">
            <arg type="u"/>
            <arg type="u"/>
        </signal>
        <signal name="ActionInvoked">
            <arg type="u"/>
            <arg type="s"/>
        </signal>
    </interface>
</node>
)""";

const Glib::ustring DBUS_KDE_STATUSNOTIFIERWATCHER = R"""(
<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="org.kde.StatusNotifierWatcher">
    <annotation name="org.gtk.GDBus.C.Name" value="SnWatcherV0Gen" />

    <method name="RegisterStatusNotifierItem">
      <annotation name="org.gtk.GDBus.C.Name" value="RegisterItem" />
      <arg type="s" direction="in" name="service" />
    </method>

    <method name="RegisterStatusNotifierHost">
      <annotation name="org.gtk.GDBus.C.Name" value="RegisterHost" />
      <arg type="s" direction="in" name="service" />
    </method>

    <property name="RegisteredStatusNotifierItems" type="as" access="read">
      <annotation name="org.gtk.GDBus.C.Name" value="RegisteredItems" />
    </property>

    <property name="IsStatusNotifierHostRegistered" type="b" access="read">
      <annotation name="org.gtk.GDBus.C.Name" value="IsHostRegistered" />
    </property>

    <property name="ProtocolVersion" type="i" access="read" />

    <signal name="StatusNotifierItemRegistered">
      <annotation name="org.gtk.GDBus.C.Name" value="ItemRegistered" />
      <arg type="s" direction="out" name="service" />
    </signal>

    <signal name="StatusNotifierItemUnregistered">
      <annotation name="org.gtk.GDBus.C.Name" value="ItemUnregistered" />
      <arg type="s" direction="out" name="service" />
    </signal>

    <signal name="StatusNotifierHostRegistered">
      <annotation name="org.gtk.GDBus.C.Name" value="HostRegistered" />
    </signal>
  </interface>
</node>
)""";

const Glib::ustring DBUS_KDE_STATUSNOTIFIERITEM = R"""(
<node>
    <interface name="org.kde.StatusNotifierItem">
        <property name="Category" type="s" access="read"/>
        <property name="Id" type="s" access="read"/>
        <property name="Title" type="s" access="read"/>
        <property name="Status" type="s" access="read"/>
        <property name="WindowId" type="i" access="read"/>
        <property name="IconThemePath" type="s" access="read"/>
        <property name="ItemIsMenu" type="b" access="read"/>
        <property name="Menu" type="o" access="read"/>
        <property name="IconName" type="s" access="read"/>
        <property name="IconPixmap" type="a(iiay)" access="read">
            <annotation name="org.qtproject.QtDBus.QtTypeName"
                        value="KDbusImageVector"/>
        </property>
        <property name="AttentionIconName" type="s" access="read"/>
        <property name="AttentionIconPixmap" type="a(iiay)" access="read">
            <annotation name="org.qtproject.QtDBus.QtTypeName"
                        value="KDbusImageVector"/>
        </property>
        <property name="ToolTip" type="(sa(iiay)ss)" access="read">
            <annotation name="org.qtproject.QtDBus.QtTypeName"
                        value="KDbusToolTipStruct"/>
        </property>
        <method name="ContextMenu">
            <arg name="x" type="i" direction="in"/>
            <arg name="y" type="i" direction="in"/>
        </method>
        <method name="Activate">
            <arg name="x" type="i" direction="in"/>
            <arg name="y" type="i" direction="in"/>
        </method>
        <method name="SecondaryActivate">
            <arg name="x" type="i" direction="in"/>
            <arg name="y" type="i" direction="in"/>
        </method>
        <method name="Scroll">
            <arg name="delta" type="i" direction="in"/>
            <arg name="orientation" type="s" direction="in"/>
        </method>
        <signal name="NewTitle"/>
        <signal name="NewIcon"/>
        <signal name="NewAttentionIcon"/>
        <signal name="NewOverlayIcon"/>
        <signal name="NewToolTip"/>
        <signal name="NewStatus">
            <arg name="status" type="s"/>
        </signal>
    </interface>
</node>
)""";

#endif

