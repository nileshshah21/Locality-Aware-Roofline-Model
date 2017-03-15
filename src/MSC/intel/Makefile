#############################################################################################
#                                              BUILD                                        #
#############################################################################################

SRC=copy.c mov.c flops.c MSC.c validation.c
OBJ=$(patsubst %.c, %.o, $(SRC))

%.o: %.c
	$(CC) -march=native $(CFLAGS) -fPIC -c -o $@ $^

%.s: %.c
	$(CC) -march=native $(CFLAGS) -S -o $@ $^

MSC.a: $(OBJ)
	ar rcs $@ $(OBJ)

clean:
	rm -rf $(OBJ) *.s MSC.a

