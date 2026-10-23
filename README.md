## Скрипты

В каталоге `scripts/` — 5 утилит. Не путайте: **две** собирают приложение,
**две** качают SDK, **одна** собирает SDK.

| Скрипт | Что делает | Когда использовать |
|---|---|---|
| `build-linux.sh` | **приложение** на Linux (fetch SDK → build → smoke) | Linux, повседневная сборка |
| `build-win.bat` | **приложение** на Windows (проверяет/собирает Core SDK → build → smoke) | Windows, повседневная сборка |
| `build-core-win.bat` | **только Core SDK** нативно на Windows | Первый раз на Windows, или для другого проекта |
| `fetch-core-sdk.sh` | **скачивает** готовый SDK с GitHub (Linux) | Нужен только SDK, без сборки приложения |
| `fetch-core-sdk.bat` | **скачивает** готовый SDK с GitHub (Windows) | То же |

### Linux

```bash
./scripts/build-linux.sh --clean --smoke
```

Скачивает SDK (`fetch-core-sdk.sh`), собирает `modbus-client`,
прогоняет smoke. **Одна команда — всё.**

### Windows

```cmd
scripts\build-win.bat --clean --smoke
```

1. Проверяет `%LOCALAPPDATA%\Core\<version>\`
2. Если SDK нет — вызывает `build-core-win.bat` (нативная сборка, ~30 мин)
3. Собирает `modbus-client.exe`
4. Прогоняет smoke

Повторные запуски — секунды (SDK уже собран).

### Отдельные сценарии

**Только скачать SDK** (без сборки приложения):

```bash
./scripts/fetch-core-sdk.sh 1.1.0 $HOME/.local/core/1.1.0    # Linux
scripts\fetch-core-sdk.bat 1.1.0 %LOCALAPPDATA%\Core\1.1.0   # Windows
```

**Только собрать Core SDK нативно** (Windows):

```cmd
scripts\build-core-win.bat 1.1.0 C:\Qt\5.15.2\mingw81_32
```

Полезно, если вы разрабатываете сам Core, или нужен SDK для другого проекта.
