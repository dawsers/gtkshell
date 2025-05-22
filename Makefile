.PHONY: all debug release clean install dev

debug:
	cmake -B ./Debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$(PREFIX)
	cmake --build ./Debug -j
	rm -rf ./compile_commands.json
	rm -rf ./gtkshell
	ln -s ./Debug/compile_commands.json .
	ln -s ./Debug/gtkshell .

release:
	cmake -B ./Release -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(PREFIX)
	cmake --build ./Release -j
	rm -rf ./compile_commands.json
	rm -rf ./gtkshell-release
	ln -s ./Release/compile_commands.json .
	ln -s ./Release/gtkshell ./gtkshell-release

all: clean release

clean:
	rm -rf Release
	rm -rf Debug
	rm -rf ./gtkshell
	rm -rf ./compile_commands.json

install: release

dev: clean debug
