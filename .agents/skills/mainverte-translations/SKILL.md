---
name: mainverte-translations
description: Add MainVerte translated text or languages, or repair its dictionary-to-C++/QML translation pipeline. Use for localization work, not unrelated UI layout changes.
---

# MainVerte translations

Use the project's embedded catalogs, not a replacement Qt Linguist workflow. Repository paths below are relative to the repository root.

- For labels or a new language, use [catalog edits](references/catalogs.md). Ordinary label additions do not require reading or changing the generator.
- For generation, language selection, persistence or live QML refresh, use [pipeline and activation](references/pipeline.md).

Keep identifiers semantic and user-facing wording consistent with nearby text. Do not edit generated headers or persist numeric `TextId` values. Reuse existing IDs only when the meaning and translation context match.

Validate the branch changed: the Python generator suite for generator behavior, generated catalogs for dictionary edits, and Qt tests/live bindings for activation changes. Report any languages or runtime behavior not verified. Do not expand a text change into an unrelated translation-system rewrite.
