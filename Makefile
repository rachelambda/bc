bc: bc.o
	$(CC) -g $^ -o $@
%.o: %.c
	$(CC) -g -c $< -o $@
