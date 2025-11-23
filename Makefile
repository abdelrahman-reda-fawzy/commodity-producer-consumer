# Compiler
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wno-write-strings

# Targets
TARGETS = producer consumer

all: $(TARGETS)

producer: producer.cpp
	$(CXX) $(CXXFLAGS) producer.cpp -o producer

consumer: consumer.cpp
	$(CXX) $(CXXFLAGS) consumer.cpp -o consumer

clean:
	rm -f $(TARGETS)
	@shmid=$$(ipcs -m | awk '/0x/ {print $$2}'); \
	if [ -n "$$shmid" ]; then \
		for id in $$shmid; do \
			ipcrm -m $$id; \
		done \
	else \
		echo "No shared memory to remove"; \
	fi

	@semid=$$(ipcs -s | awk '/0x/ {print $$2}'); \
	if [ -n "$$semid" ]; then \
		for id in $$semid; do \
			ipcrm -s $$id; \
		done \
	fi
