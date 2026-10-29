from pathlib import Path
import subprocess,json
root=Path(__file__).resolve().parent.parent
result=json.loads((root/'dist'/'installation-verification.json').read_text(encoding='utf-8'))
directory=Path(result['installDirectory'])
log=root.parent/'delivery'/'installed-client-launch.log'
with log.open('wb') as output:
 child=subprocess.Popen([str(directory/'ShadowChat.exe')],cwd=directory,stdin=subprocess.DEVNULL,stdout=output,stderr=output)
(root.parent/'delivery'/'installed-client-process.json').write_text(json.dumps({'pid':child.pid,'executable':str(directory/'ShadowChat.exe'),'log':str(log)}),encoding='utf-8')
print('Launched installed test client PID',child.pid)
