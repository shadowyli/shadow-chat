from pathlib import Path
import subprocess

SOURCE_DIRECTORIES = {
    'src', 'tests', 'docs', 'packaging', 'scripts', 'branding', 'locale',
    'design-system', '.github', 'dev-preview', 'misc',
}
SOURCE_SUFFIXES = {
    '.cpp', '.h', '.hpp', '.pro', '.pri', '.py', '.ps1', '.cmd', '.bat',
    '.sh', '.js', '.mjs', '.ts', '.tsx', '.vue', '.qml', '.json', '.yml',
    '.yaml', '.xml', '.qrc', '.ui', '.md', '.qss',
}
PRIVATE_SEGMENTS = {'.git', '.tools', 'dist', 'delivery-private', '__pycache__'}


def select_source_files(root, approved_new):
    root = Path(root).resolve()
    if not isinstance(approved_new, list) or any(not isinstance(name, str) for name in approved_new):
        raise ValueError('Reviewed new sources must be a JSON array of relative paths')
    if len(approved_new) != len(set(approved_new)):
        raise ValueError('Duplicate reviewed new source paths')

    def names(*arguments):
        result = subprocess.check_output(['git', 'ls-files', '-z', *arguments], cwd=root)
        return set(result.decode('utf-8').split('\0'))-{''}

    tracked = names('--cached')
    untracked = names('--others', '--exclude-standard')
    approved = set(approved_new)
    if not approved.issubset(untracked):
        raise ValueError('Reviewed new sources must identify currently untracked files')
    for name in approved:
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts or '\\' in name or not relative.parts:
            raise ValueError('Unsafe reviewed source path')
        if relative.parts[0] not in SOURCE_DIRECTORIES:
            raise ValueError('New source must be inside a reviewed source directory: '+name)
    unreviewed = {
        name for name in untracked-approved
        if Path(name).parts[0] in SOURCE_DIRECTORIES or Path(name).suffix.lower() in SOURCE_SUFFIXES
    }
    if unreviewed:
        raise ValueError('Unreviewed new source files: '+', '.join(sorted(unreviewed)))

    selected = []
    for name in sorted(tracked | approved):
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts or PRIVATE_SEGMENTS.intersection(relative.parts):
            raise ValueError('Private or generated source entry rejected: '+name)
        source = root/relative
        if not source.exists():
            if name in approved:
                raise ValueError('Reviewed new source missing: '+name)
            continue
        if source.is_symlink() or not source.resolve().is_relative_to(root):
            raise ValueError('Linked or out-of-workspace source rejected: '+name)
        if not source.is_file():
            raise ValueError('Source entry is not a regular file: '+name)
        selected.append((name, source))
    return selected, len(untracked-approved)
