CC = gcc
CFLAGS = -g

OBJS_CLIENT = cirl.o cirlcommon.o led.o tomlc99/toml.o
OBJS_SERVER = cirlservice.o cirlcommon.o
$(OBJS_CLIENT) $(OBJS_SERVER): cirlcommon.h

TOMLMAKE = $(MAKE) -C tomlc99

.PHONY: cirl cirlserv install_cirl uninstall_cirl install_cirlserv uninstall_cirlserv clean
cirl: $(OBJS_CLIENT)
	$(TOMLMAKE)
	cc -o cirl $(OBJS_CLIENT) -lpthread -lm
cirlserv: $(OBJS_SERVER)
	cc -o cirlserv $(OBJS_SERVER) -lpthread -lm
install_cirl:
	sudo mv cirl /usr/local/bin/
	sudo cp cirl.toml ~/.config/cirl/
uninstall_cirl:
	sudo rm /usr/local/bin/cirl
install_cirlserv:
	sudo mv cirlserv /usr/local/bin
uninstall_cirlserv:
	sudo rm /usr/local/bin/cirlserv
clean:
	rm $(OBJS_CLIENT) cirlserv.o
