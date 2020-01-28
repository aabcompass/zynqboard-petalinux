APP = artix-load-uapp

# Add any other object files to this list below
APP_OBJS = artix-load-uapp.o

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS)

