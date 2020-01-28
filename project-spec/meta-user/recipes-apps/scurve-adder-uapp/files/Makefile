APP = scurve-adder-uapp

# Add any other object files to this list below
APP_OBJS = scurve-adder-uapp.o

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS)

