package main

import (
	"log"

	"github.com/abiosoft/ishell"

	"github.com/jrozner/the-intercept-2019/web/client"
)

func main() {
	config, err := readConfig()
	if err != nil {
		log.Println("no config file found; creating a new one")
		config, err = createConfig()
		if err != nil {
			log.Fatal("unable to create new config")
		}
	}

	c, err := client.NewClient(config.Host, config.AccessKey, config.SecretKey)
	if err != nil {
		log.Fatal(err)
	}

	shell := ishell.New()
	shell.Set("client", c)
	shell.Set("config", config)

	shell.AddCmd(&ishell.Cmd{
		Name: "register",
		Help: "register a new user",
		Func: register,
	})

	shell.AddCmd(&ishell.Cmd{
		Name: "contacts",
		Help: "list contacts",
		Func: listContacts,
	})

	shell.AddCmd(&ishell.Cmd{
		Name: "compose",
		Help: "compose a new message",
		Func: compose,
	})

	shell.AddCmd(&ishell.Cmd{
		Name: "messages",
		Help: "list all messages",
		Func: allMessages,
	})

	shell.AddCmd(&ishell.Cmd{
		Name: "unread",
		Help: "list all unread messages",
		Func: unreadMessages,
	})

	shell.AddCmd(&ishell.Cmd{
		Name: "read",
		Help: "read a message",
		Func: readMessage,
	})

	shell.AddCmd(&ishell.Cmd{
		Name: "host",
		Help: "sets the host",
		Func: setHost,
	})

	shell.AddCmd(&ishell.Cmd{
		Name: "config",
		Help: "shows the current config",
		Func: showConfig,
	})

	shell.AddCmd(&ishell.Cmd{
		Name: "rotate",
		Help: "rotate secret key",
		Func: rotateSecret,
	})

	shell.Run()
}
