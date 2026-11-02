<p align="center">
  <img src="branding/generated/shadowchat-128.png" width="80" height="80" alt="ShadowChat icon">
</p>

<h1 align="center">ShadowChat · 影聊</h1>
<p align="center">Direct conversations on the same local network.</p>

<p align="center">
  <a href="LICENSE.txt"><img src="https://img.shields.io/badge/license-GPLv3-blue" alt="GPLv3 license"></a>
  <img src="https://img.shields.io/badge/platform-Windows%2010%20%2F%2011-0078D4" alt="Windows 10 and 11">
  <img src="https://img.shields.io/badge/status-preview-d97706" alt="Preview">
</p>

<p align="center">
  <a href="https://github.com/shadowyli/shadow-chat/releases">Download</a> ·
  <a href="docs/GETTING-STARTED.md">User guide</a> ·
  <a href="CONTRIBUTING.md">Contribute</a> ·
  <a href="README.md">简体中文</a>
</p>

ShadowChat is an open-source Windows messenger for office LANs and small teams on
the same subnet. It provides private and group conversations and file transfers
without deploying a central chat server. Chat history and account data stay on your computer.

Maintained by **shadowyli**, the project builds on **BeeBEEP r1557** with C++ / Qt
improvements to conversation organization, message presentation, and everyday interactions.

## Features

- **LAN conversations**: direct communication with private chats, groups, mentions, and replies.
- **File sharing**: images, files, and folders, with a shared preview and send flow for multiple images.
- **Local organization**: favorite conversations, static unread indicators, global search, and history filters.
- **Data migration**: separate local account data and workspace export / import for moving to another computer.

## Get started

**Preview packages for Windows 10/11 x86** include the required runtime components.

1. Download `*-setup.exe` from [Releases](https://github.com/shadowyli/shadow-chat/releases),
   read the release notes, and check the file against `SHA256SUMS.txt`.
2. Install and create a local account. Exit ShadowChat from the system tray before upgrading.
3. Start ShadowChat on another computer on the same LAN, wait for discovery, and open a conversation.

See the [user guide](docs/GETTING-STARTED.md) for details. Each release records its
changes, validation scope, and known limitations. Current preview packages are not commercially code-signed.

Your network must allow communication between devices. Email addresses are local identifiers;
there is no central identity service or cloud history sync. Local data and backups are
unencrypted and need protection. See [security](SECURITY.md).

## Documentation

| Topic | Guide |
| --- | --- |
| Install, upgrade, back up, and migrate | [User guide](docs/GETTING-STARTED.md) |
| Accounts, offline queues, and network troubleshooting | [FAQ](docs/FAQ.md) |
| Windows source builds, tests, and packaging | [Build guide](docs/BUILDING.md) |
| Questions and bug reports | [Support](SUPPORT.md) |

## Contribute

Documentation fixes, translations, reproducible reports, and code contributions are welcome.
Read [contributing](CONTRIBUTING.md), use [Issues](https://github.com/shadowyli/shadow-chat/issues)
for reports and proposals, and follow the [code of conduct](CODE_OF_CONDUCT.md).
Chinese and English feedback are welcome; the application UI and detailed guides are primarily in Chinese.

## License and credits

[GNU GPLv3](LICENSE.txt), with upstream copyright notices preserved.
Thanks to **Marco Mastroddi** and [BeeBEEP](https://www.beebeep.net/) contributors
for the LAN messaging foundation, and to Qt, Phosphor Icons, and other components.
See [third-party notices](THIRD_PARTY_NOTICES.md); runtime versions and licenses
are included with the corresponding release artifacts.
