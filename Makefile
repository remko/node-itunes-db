BINDINGS=build/Release/itunes_db.node

.PHONY: all
all: $(BINDINGS)


.PHONY: clean
clean:
	-rm -rf build

.PHONY: lint
lint:
	./node_modules/.bin/eslint .

ifneq ($(CI),1)
TAPE_SINK=| ./node_modules/.bin/faucet
endif

.PHONY: check
check: $(BINDINGS)
ifeq ($(COVERAGE),1)
	node_modules/.bin/istanbul cover test/*.js
else
	node_modules/.bin/tape 'test/**/*.js' $(TAPE_SINK)
endif

.PHONY: publish
publish:
	npm publish

################################################################################
# Bindings
################################################################################

build/Makefile: binding.gyp
	node-gyp configure

$(BINDINGS): build/Makefile $(wildcard *.cpp)
	node-gyp build
