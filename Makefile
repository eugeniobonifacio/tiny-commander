###############################################################################
# Makefile per Tiny Commander
###############################################################################

# Nome del programma
PROG = tyc

# Compilatore
CC = gcc

# File sorgente
SRC = tyc.c

# Estensione oggetto (dipende dalla piattaforma)
OBJ = $(SRC:.c=.o)

# Flags di compilazione
CFLAGS = -Wall -Wextra -pedantic -O2

# Percorso della libreria ncurses statica (può essere ridefinito dall'utente)
NCURSES_STATIC_PATH ?= ../static-lib/libncurses-src/lib

# Librerie
# Utilizzo dinamico di ncurses per la build normale
LIBS = -lncurses

# Libreria statica ncurses per build statica
NCURSES_STATIC_LIB = $(NCURSES_STATIC_PATH)/libncursesw.a

# Determina il sistema operativo
UNAME := $(shell uname)

# Determina la versione da git
GIT_TAG := $(shell git describe --tags --exact-match 2>/dev/null)
GIT_BRANCH := $(shell git rev-parse --abbrev-ref HEAD 2>/dev/null)
GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null)

# Se è un tag, usa solo il tag come versione
ifneq ($(GIT_TAG),)
	GIT_VERSION := $(GIT_TAG)
# Altrimenti, usa branch + hash
else
	GIT_VERSION := $(GIT_BRANCH)-$(GIT_HASH)
endif

# Aggiunge la definizione di GIT_VERSION durante la compilazione
CFLAGS += -DGIT_VERSION='"$(GIT_VERSION)"'

# Configurazione specifica per macOS
ifeq ($(UNAME), Darwin)
	# Verifica se homebrew è in /opt/homebrew (Apple Silicon) o /usr/local (Intel)
	ifneq ($(wildcard /opt/homebrew/.*),)
		# Percorso homebrew per Apple Silicon
		BREW_PREFIX = /opt/homebrew
	else
		# Percorso homebrew per Intel
		BREW_PREFIX = /usr/local
	endif
	
	# Include e librerie per macOS
	CFLAGS += -I$(BREW_PREFIX)/include
	LDFLAGS += -L$(BREW_PREFIX)/lib
	
	# Aggiungi flag per supportare sia Intel che Apple Silicon
	CFLAGS += -arch x86_64 -arch arm64
endif

# Configurazione per Linux
ifeq ($(UNAME), Linux)
	# Verifica se siamo su Alpine (per musl)
	ifneq ($(wildcard /etc/alpine-release),)
		# Su Alpine, supportiamo la compilazione statica
		STATIC_SUPPORTED = 1
	endif
endif

# Target predefinito
all: $(PROG)

# Compilazione normale
$(PROG): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

# Regola per generare file oggetto
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

# Pulizia
clean:
	rm -f $(PROG) $(PROG)_static $(PROG)_musl $(PROG)_alpine $(OBJ) 

# Installazione
install: $(PROG)
	mkdir -p $(DESTDIR)/usr/local/bin
	cp $(PROG) $(DESTDIR)/usr/local/bin/

# Compilazione statica con libreria ncurses personalizzata
static-custom: $(SRC)
	$(CC) $(CFLAGS) -o $(PROG)_static $< $(NCURSES_STATIC_LIB)

# Compilazione statica (se supportata)
static: $(SRC)
ifeq ($(STATIC_SUPPORTED), 1)
	$(CC) $(CFLAGS) -o $(PROG)_static $< -static $(LIBS)
else
	@echo "La compilazione statica con -static non è supportata su questa piattaforma"
	@echo "Prova 'make static-custom' per linkare direttamente la libreria ncurses statica"
endif

# Compilazione per debug
debug: CFLAGS += -g -DDEBUG
debug: clean $(PROG)

# Target con musl (solo su Linux)
musl: $(SRC)
ifeq ($(UNAME), Linux)
	@if command -v musl-gcc > /dev/null; then \
		musl-gcc $(CFLAGS) -o $(PROG)_musl $< $(LIBS); \
	else \
		echo "musl-gcc non trovato. Installalo con 'apt-get install musl-tools' o equivalente."; \
	fi
else
	@echo "musl è supportato solo su Linux"
endif

# Target per Docker (funziona su qualsiasi sistema con Docker)
docker:
	docker run --rm -v "$(PWD):/src" -w /src alpine:latest sh -c "apk add --no-cache build-base ncurses-dev ncurses-static && gcc -o $(PROG)_alpine $(SRC) -lncurses -static"

# Versione
version:
	@echo "Tiny Commander v0.1"
	@echo "Compilatore: $(CC) $(shell $(CC) --version | head -n 1)"
	@echo "Sistema: $(UNAME)"
	@echo "Percorso libreria ncurses statica: $(NCURSES_STATIC_PATH)"

# Phony targets
.PHONY: all clean install static static-custom debug musl docker version
