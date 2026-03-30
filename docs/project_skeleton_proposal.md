# Предложение: начальный каркас проекта (C++ + CMake)

## 1) Цели каркаса
- Быстро запустить MVP-команды (`scan`, `fix`, `backup`, `restore`) без переписывания структуры позже.
- Сразу заложить разделение ответственности из ТЗ: CLI, реестр, системные команды, бизнес-логика, отчёты.
- Обеспечить удобную сборку через CMake на Windows (MSVC), с возможностью подключить тесты.

## 2) Предлагаемая структура каталогов

```text
ghost-layout-fixer/
├─ CMakeLists.txt
├─ cmake/
│  └─ warnings.cmake
├─ src/
│  ├─ main.cpp
│  ├─ cli/
│  │  ├─ CliParser.h
│  │  ├─ CliTypes.h
│  │  └─ src/
│  │     └─ CliParser.cpp
│  ├─ core/
│  │  ├─ LayoutFixService.h
│  │  ├─ Models.h
│  │  ├─ ExitCodes.h
│  │  └─ src/
│  │     └─ LayoutFixService.cpp
│  ├─ platform/
│  │  ├─ RegistryService.h
│  │  ├─ SystemCommandRunner.h
│  │  ├─ PrivilegeService.h
│  │  ├─ InstalledLanguageService.h
│  │  └─ src/
│  │     ├─ RegistryService.cpp
│  │     ├─ SystemCommandRunner.cpp
│  │     ├─ PrivilegeService.cpp
│  │     └─ InstalledLanguageService.cpp
│  ├─ report/
│  │  ├─ ReportPrinter.h
│  │  └─ src/
│  │     └─ ReportPrinter.cpp
│  └─ common/
│     ├─ Logger.h
│     ├─ Result.h
│     ├─ StringUtils.h
│     └─ src/
│        └─ Logger.cpp
├─ tests/
│  ├─ unit/
│  │  ├─ CliParserTests.cpp
│  │  └─ LayoutFixServiceTests.cpp
│  └─ fakes/
│     ├─ FakeRegistryService.h
│     └─ FakeCommandRunner.h
├─ scripts/
│  ├─ run_scan.ps1
│  └─ run_fix_dry.ps1
└─ docs/
   ├─ ghost-layout-fixer_MVP.md
   ├─ ghost-layout-fixer_TZ.md
   └─ project_skeleton_proposal.md
```

## 3) Слои и зависимости

Правило зависимостей (сверху вниз):
1. `cli` ->
2. `core` ->
3. `platform` / `report` / `common`

Ограничения:
- `platform` не зависит от `cli`.
- `report` не меняет состояние системы (только форматирует и печатает).
- `core` работает через интерфейсы (`IRegistryService`, `ICommandRunner`) для тестируемости.
- Заголовки и реализации храним внутри `src/<module>/` (например, `src/cli/CliParser.h` и `src/cli/src/CliParser.cpp`).

## 4) Минимальный набор сущностей

### CLI
- `CommandType { Scan, Fix, Backup, Restore }`
- `CliOptions { command, layoutCode, dryRun, restoreFile, assumeYes }`

### Core
- `ScanResult`
  - список проверенных веток
  - найденные совпадения
  - итоговый статус
- `FixPlan`
  - шаги dry-run
  - какие ключи потенциально меняются
- `FixReport`
  - выполненные шаги
  - ошибки
  - путь backup

### Platform
- `RegistryLocation { hive, path }`
- `RegistryMatch { location, valueName, valueData }`
- `CommandResult { exitCode, stdoutText, stderrText }`

## 5) Компоненты MVP и их ответственность

- `CliParser`
  - валидация команд и флагов;
  - поддержка только `en-GB` на этапе MVP.

- `LayoutFixService`
  - orchestration сценариев `scan/fix/backup/restore`;
  - принудительный backup перед изменениями;
  - повторный scan после fix.

- `RegistryService`
  - чтение whitelist-веток из ТЗ;
  - поиск следов layout;
  - удаление только релевантных записей.

- `SystemCommandRunner`
  - запуск `reg.exe` / PowerShell;
  - захват `stdout/stderr/exit code`.

- `PrivilegeService`
  - проверка admin-прав перед mutating-операциями.

- `InstalledLanguageService`
  - получение списка реально установленных языков/раскладок в системе.

- `ReportPrinter`
  - единый читаемый вывод;
  - форматы для `scan`, `dry-run`, `fix`, `restore`.

## 6) Структура CMake (предложение)

- Один исполняемый target: `ghost-layout-fixer`.
- Опциональный target тестов при `-DBUILD_TESTS=ON`.
- Общие предупреждения компилятора:
  - MSVC: `/W4 /permissive-`
  - Clang/GCC (на будущее): `-Wall -Wextra -Wpedantic`
- Стандарт C++: `CMAKE_CXX_STANDARD 20`.

Рекомендуемые опции:
- `BUILD_TESTS` (ON/OFF)

## 7) Соглашения по коду
- Пространство имён: `ghost`.
- Имена файлов: `PascalCase` для интерфейсных классов.
- Локальные переменные: `camelCase`.
- Глобальные переменные: `camelCase_`.
- Ошибки: через явный `Result<T, Error>` или коды ошибок, без скрытия контекста.
- Логи: сначала в консоль, файловый лог — post-MVP.

## 8) Поэтапный старт без реализации логики

Этап 1 (скелет):
- создать директории и заглушки классов/интерфейсов;
- настроить CMake и сборку пустого CLI;
- зависимости на старте не обязательны; если появятся внешние библиотеки, подключаем через `conan2` (минимально и только по необходимости);
- добавить enum команд и exit-коды.

Этап 2 (read-only):
- реализовать `scan` только на чтение c выявлением ghost-раскладок
  (есть в реестре, но отсутствуют в списке установленных языков);
- оформить отчёт.
- для локального теста без реестра/PowerShell поддержать mock-переменные:
  `GLF_MOCK_REGISTRY_LAYOUTS` и `GLF_MOCK_INSTALLED_LAYOUTS`.

Этап 3 (safe mutate):
- реализовать `backup` и `fix --dry-run`;
- затем реальный `fix` с confirm + backup.

Этап 4 (restore + polish):
- `restore --file`;
- доработка сообщений, кодов возврата и тестов.

## 9) Риски и как закрыть в каркасе
- Риск: нестабильность внешних команд PowerShell.
  - Митигировать: `SystemCommandRunner` + строгий разбор `exit code`.
- Риск: случайное удаление лишних ключей.
  - Митигировать: whitelist веток + фильтр по `layoutCode` + dry-run.
- Риск: низкая тестируемость WinAPI-кода.
  - Митигировать: интерфейсы и fake-реализации в `tests/fakes`.

## 10) Что НЕ делаем сейчас
- Не реализуем фактическое изменение реестра.
- Не добавляем GUI/сервис/автозапуск.
- Не расширяем поддержку beyond `en-GB` до завершения MVP.
