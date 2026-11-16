.PHONY: all clean
all:
	cmake -S . -B build.aurore -DAURORE_PLATFORM=SWITCH -DAURORE_GRAPHICS=DEKO3D -DCMAKE_BUILD_TYPE=Release
	cmake --build build.aurore --parallel 4
	cp build.aurore/navidrome_nx.nro navidrome_nx.nro
clean:
	rm -rf build.aurore navidrome_nx.nro
