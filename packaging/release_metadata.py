import re


def update_metadata(receipt, source_commit, installer_name):
    version = receipt["version"]
    environment = receipt["releaseEnvironment"]
    channel = receipt["channel"]
    identity = receipt["productionBuild"]["binaryBuildIdentity"]
    expected_channel = "stable" if environment == "main" else "test"
    expected_name = f"ShadowChat-{version}-{environment}-setup.exe"
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError("Invalid release version")
    if environment not in {"local", "dev", "main"} or channel != expected_channel:
        raise ValueError("Environment/channel mismatch")
    if installer_name != expected_name:
        raise ValueError("Public installer name is incompatible with updater")
    if not re.fullmatch(r"[0-9a-f]{40}", source_commit):
        raise ValueError("Exact source commit is required")
    if not re.fullmatch(r"[0-9a-f]{64}", receipt["sha256"]):
        raise ValueError("Installer checksum is required")
    if not isinstance(receipt["size"], int) or isinstance(receipt["size"], bool) or not 0 < receipt["size"] <= 2**31:
        raise ValueError("Invalid installer size")
    for key, expected in {"version": version, "environment": environment, "channel": channel}.items():
        if identity.get(key) != expected:
            raise ValueError("Executable identity differs from metadata")
    tag = f"v{version}" + ("-test" if channel == "test" else "")
    return {
        "product": "ShadowChat",
        "version": version,
        "sourceTag": tag,
        "sourceCommit": source_commit,
        "releaseEnvironment": environment,
        "channel": channel,
        "binaryBuildIdentity": identity,
        "installer": {"file": expected_name, "bytes": receipt["size"], "sha256": receipt["sha256"]},
    }
