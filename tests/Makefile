# Host-side unit tests and dashboard previews for IAQ-MK2.
#   make        build + run tests
#   make preview render dashboard PNGs into out/
CXX      ?= g++
SKETCH   := ../iaq-mk2
VENDOR   := $(SKETCH)/src/waveshare
CXXFLAGS := -std=c++17 -O1 -g -Wall -Wextra -Werror -I$(SKETCH) -I$(VENDOR) -DIAQ_HOST_BUILD
VENDOR_FLAGS := -std=c++17 -O1 -g -w -I$(SKETCH) -I$(VENDOR) -DIAQ_HOST_BUILD

PURE_SRC   := $(wildcard $(SKETCH)/text_format.cpp $(SKETCH)/air_status.cpp $(SKETCH)/pms7003.cpp $(SKETCH)/sen0219.cpp $(SKETCH)/dashboard.cpp $(SKETCH)/readiness.cpp)
VENDOR_SRC := $(wildcard $(VENDOR)/GUI_Paint.cpp $(VENDOR)/font8.cpp $(VENDOR)/font12.cpp $(VENDOR)/font16.cpp $(VENDOR)/font20.cpp $(VENDOR)/font24.cpp)
TEST_SRC   := $(wildcard test_*.cpp)

PURE_OBJ   := $(patsubst $(SKETCH)/%.cpp,build/pure_%.o,$(PURE_SRC))
VENDOR_OBJ := $(patsubst $(VENDOR)/%.cpp,build/vendor_%.o,$(VENDOR_SRC))
TEST_OBJ   := $(patsubst %.cpp,build/%.o,$(TEST_SRC))

.PHONY: all test preview clean
all: test

test: build/test_runner
	./build/test_runner

preview: build/render_preview
	mkdir -p out
	./build/render_preview out
	uv run --quiet ppm_to_png.py out

build:
	mkdir -p build

build/pure_%.o: $(SKETCH)/%.cpp | build
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/vendor_%.o: $(VENDOR)/%.cpp | build
	$(CXX) $(VENDOR_FLAGS) -c -o $@ $<

build/%.o: %.cpp | build
	$(CXX) $(CXXFLAGS) -c -o $@ $<

build/test_runner: $(TEST_OBJ) $(PURE_OBJ) $(VENDOR_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/render_preview: build/render_preview.o $(PURE_OBJ) $(VENDOR_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -rf build out

# Host tools (Python): parser tests for tools/serial_dump.py
.PHONY: tools
tools:
	cd ../tools && uv run --quiet --with pytest --with pyserial --with typer python -m pytest -q .
