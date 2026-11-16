import json
import re
import string
from pathlib import Path


def flatten(value, prefix=""):
    result = {}
    for key, item in value.items():
        path = prefix + key
        if isinstance(item, dict):
            result.update(flatten(item, path + "/"))
        else:
            assert isinstance(item, str) and item, path
            result[path] = item
    return result


catalogs = {
    locale: flatten(json.loads((Path("resources/i18n") / locale / "app.json").read_text()))
    for locale in ("en-US", "es")
}
english = catalogs["en-US"]
formatter = string.Formatter()
for locale, catalog in catalogs.items():
    assert catalog.keys() == english.keys(), locale
    for key, text in catalog.items():
        fields = sorted(field for _, field, _, _ in formatter.parse(text) if field is not None)
        expected = sorted(field for _, field, _, _ in formatter.parse(english[key]) if field is not None)
        assert fields == expected, (locale, key)

source = "\n".join(path.read_text() for path in Path("src").glob("*.*") if path.suffix in {".cpp", ".hpp"})
for key in re.findall(r'"app/([a-z_]+)"', source):
    assert key in english, key
api = Path("src/api.hpp").read_text()
for key in re.findall(r'ApiError\("([a-z_]+)"', api):
    assert "error/" + key in english, key

print("Translation keys and format placeholders passed")
