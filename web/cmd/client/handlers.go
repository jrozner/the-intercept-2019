package main

import (
	"log"
	"strconv"

	"github.com/abiosoft/ishell"

	"github.com/jrozner/the-intercept-2019/web/client"
)

func register(ctx *ishell.Context) {
	c, ok := ctx.Get("client").(*client.Client)
	if !ok {
		log.Panic("no client exists")
	}

	ctx.Print("enter your team name: ")
	team := ctx.ReadLine()

	ctx.Print("enter your serial number: ")
	serial := ctx.ReadLine()

	keys, err := c.Register(team, serial)
	if err != nil {
		ctx.Printf("unable to register: %s\n", err)
		return
	}

	config, ok := ctx.Get("config").(*Config)
	if !ok {
		log.Panic("no config exists")
	}

	config.AccessKey = keys.AccessKey
	config.SecretKey = keys.SecretKey
	config.Serial = serial
	c.SetAccessKey(config.AccessKey)
	c.SetSerial(config.Serial)
	err = c.SetSecretKey(config.SecretKey)
	if err != nil {
		log.Panic(err)
	}

	err = saveConfig(config)
	if err != nil {
		ctx.Println(err)
	}
}

func listContacts(ctx *ishell.Context) {
	c, ok := ctx.Get("client").(*client.Client)
	if !ok {
		log.Panic("no client exists")
	}

	contacts, err := c.GetContacts()
	if err != nil {
		ctx.Println(err)
		return
	}

	ctx.Println("Contacts:")
	for _, contact := range contacts {
		ctx.Println(contact.TeamName)
	}
}

func compose(ctx *ishell.Context) {
	c, ok := ctx.Get("client").(*client.Client)
	if !ok {
		log.Panic("no client exists")
	}

	ctx.Print("TO: ")
	receiver := ctx.ReadLine()
	ctx.Printf("BODY:")
	message := ctx.ReadLine()

	err := c.SendMessage(receiver, message)
	if err != nil {
		ctx.Println(err)
	}
}

func unreadMessages(ctx *ishell.Context) {
	c, ok := ctx.Get("client").(*client.Client)
	if !ok {
		log.Panic("no client exists")
	}

	messages, err := c.GetUnreadMessages()
	if err != nil {
		ctx.Println(err)
		return
	}

	for _, message := range messages {
		ctx.Printf("From %s\n\n%s\n\n----------------------------------------------\n", message.Sender.TeamName, message.Text)
	}

}

func allMessages(ctx *ishell.Context) {
	c, ok := ctx.Get("client").(*client.Client)
	if !ok {
		log.Panic("no client exists")
	}

	messages, err := c.GetAllMessages()
	if err != nil {
		ctx.Println(err)
		return
	}

	for _, message := range messages {
		ctx.Printf("From %s\n\n%s\n\n----------------------------------------------\n", message.Sender.TeamName, message.Text)
	}
}

func readMessage(ctx *ishell.Context) {
	c, ok := ctx.Get("client").(*client.Client)
	if !ok {
		log.Panic("no client exists")
	}

	ctx.Print("enter the message id: ")
	idString := ctx.ReadLine()

	id, err := strconv.ParseUint(idString, 10, 64)
	if err != nil {
		ctx.Println(err)
		return
	}

	message, err := c.GetMessage(id)
	if err != nil {
		ctx.Println(err)
		return
	}

	ctx.Printf("From %s\n\n%s\n\n----------------------------------------------\n", message.Sender.TeamName, message.Text)
}

func rotateSecret(ctx *ishell.Context) {
	c, ok := ctx.Get("client").(*client.Client)
	if !ok {
		log.Panic("no client exists")
	}

	keys, err := c.RotateSecret()
	if err != nil {
		ctx.Println(err)
		return
	}

	config, ok := ctx.Get("config").(*Config)
	if !ok {
		log.Panic("no config exists")
	}

	config.SecretKey = keys.SecretKey
	err = c.SetSecretKey(config.SecretKey)
	if err != nil {
		log.Panic(err)
	}

	err = saveConfig(config)
	if err != nil {
		ctx.Println(err)
	}
}

func setHost(ctx *ishell.Context) {
	c, ok := ctx.Get("client").(*client.Client)
	if !ok {
		log.Panic("no client exists")
	}

	ctx.Print("Enter the host to communicate with: ")
	host := ctx.ReadLine()

	config, ok := ctx.Get("config").(*Config)
	if !ok {
		log.Panic("no config exists")
	}

	c.SetHost(host)
	config.Host = host
	err := saveConfig(config)
	if err != nil {
		log.Panic(err)
	}
}

func showConfig(ctx *ishell.Context) {
	config, ok := ctx.Get("config").(*Config)
	if !ok {
		log.Panic("no config exists")
	}

	ctx.Printf("Host: %s\n", config.Host)
	ctx.Printf("Access Key: %s\n", config.AccessKey)
	ctx.Printf("Secret Key: %s\n", config.SecretKey)
}
