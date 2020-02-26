APP = dma-uapp

# Add any other object files to this list below
APP_OBJS = dma-uapp.o

# Andrey Poroshin added pthread library support
LDLIBS += -lpthread

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS)

