Source: https://github.com/JigSawFr/Aurore
Release: v1.1.0
Revision: e8ea3dcfc194c9b0271f064ba67cfe462c9baad1
Core: Apache-2.0. Cadran: GPL-3.0-or-later; see LICENSE.GPL and NOTICE.
Platform-specific optional submodules are omitted; Switch builds use devkitPro's
installed libraries. Local patches, if needed, are documented in docs/aurore-migration.md.
The harness's PNG writer is deps/stb_image_write.h from the pinned GLFW
submodule revision fd162259625651250feb3f382130bd0af4a6337e; its included
notices and GLFW's LICENSE.md are retained. No GLFW source is built.
