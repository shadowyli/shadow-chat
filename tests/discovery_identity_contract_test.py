from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def source(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise AssertionError(f"missing {label}: {fragment}")


def main() -> None:
    protocol = source("src/core/Protocol.cpp")
    broadcaster = source("src/core/Broadcaster.cpp")
    socket = source("src/core/ConnectionSocket.cpp")
    core_chat = source("src/core/CoreChat.cpp")
    core_parser = source("src/core/CoreParser.cpp")
    network = source("src/core/NetworkManager.cpp")
    user_list = source("src/desktop/GuiUserList.cpp")
    directory = source("src/desktop/ShadowDirectoryPage.cpp")
    search = source("src/desktop/ShadowSearchPanel.cpp")
    group_picker = source("src/desktop/GuiCreateGroup.cpp")
    chat_list = source("src/desktop/GuiChatList.cpp")
    main_window = source("src/desktop/GuiMain.cpp")

    require(protocol, 'QStringLiteral( "ShadowChat/1" )', "identity marker")
    require(protocol, "text_fields << SHADOWCHAT_IDENTITY_MARKER", "discovery identity")
    require(protocol, "data_list << SHADOWCHAT_IDENTITY_MARKER", "HELLO identity")
    require(protocol, "!isShadowChatNetworkAddress( ur.networkAddress() )", "cache identity gate")
    require(broadcaster, "isShadowChatBroadcastMessage( m )", "broadcast rejection")
    require(broadcaster, "const QHostAddress primary_broadcast", "broadcast fallback")
    require(socket, "isShadowChatHelloMessage( m )", "HELLO rejection")
    require(core_chat, "isShadowChatUser( to_user )", "message delivery gate")
    require(network, "const QList<NetworkEntry> current_network_entries = availableNetworkEntries()", "live interface check")
    require(user_list, "isShadowChatUser( u )", "legacy list visibility gate")
    require(directory, "isShadowChatUser( user )", "directory visibility gate")
    require(search, "isShadowChatUser( user )", "global search visibility gate")
    require(group_picker, "isShadowChatUser( u )", "group picker visibility gate")
    require(chat_list, "c.privateUserId()", "recent private chat identity gate")
    require(main_window, "Refused to open a private chat without a verified ShadowChat peer", "private chat open gate")

    if "createTemporaryUser( ur )" in core_parser:
        raise AssertionError("group discovery can still create an unverified temporary user")

    hello_guard = socket.index("isShadowChatHelloMessage( m )")
    workgroup_guard = socket.index("acceptConnectionsOnlyFromWorkgroups()")
    if hello_guard >= workgroup_guard:
        raise AssertionError("ShadowChat identity must be checked before workgroup admission")

    print("ShadowChat discovery identity contract: PASS")


if __name__ == "__main__":
    main()
