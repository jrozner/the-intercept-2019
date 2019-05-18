package main

import (
	"bytes"
	"fmt"
	"log"
	"strings"
	"time"

	"github.com/jrozner/the-intercept-2019/web/client"
)

var flags map[string]Flag

func main() {
	config, err := readConfig()
	if err != nil {
		log.Fatal(err)
	}

	flags, err = readFlags()
	if err != nil {
		log.Fatal(err)
	}

	c, err := client.NewClient(config.Host, config.AccessKey, config.SecretKey, config.Serial)
	if err != nil {
		log.Fatal(err)
	}

	for {
		messages, err := c.GetUnreadMessages()
		if err != nil {
			log.Println(err)
			continue
		}

		for _, message := range messages {
			commandIndex := strings.Index(message.Text, " ")
			recipient := message.Sender.TeamName

			var command string
			var remainder string
			if commandIndex == -1 {
				command = message.Text
			} else {
				command = message.Text[0:commandIndex]
				remainder = message.Text[commandIndex:]
			}

			switch command {
			case "help":
				sendHelp(c, recipient)
			case "challenges":
				sendChallenges(c, recipient)
			case "challenge":
				sendChallenge(c, recipient, remainder)
			case "solve":
			case "scoreboard":
			}

			err = c.DeleteMessage(message.ID)
			if err != nil {
				log.Println("unable to delete message: ", err)
			}
		}

		time.Sleep(10 * time.Second)
	}
}

func sendHelp(c *client.Client, recipient string) {
	response := fmt.Sprintf("I know the following commands:\n\nhelp - this message\nchallenges - list all challenges\nchallenge <name> - list challenge details\nsolve <name>:<flag> - solve a challenge\nscoreboard - list the scoreboard")
	err := c.SendMessage(recipient, response)
	if err != nil {
		log.Println("failed to respond to user: ", err)
	}
}

func sendChallenges(c *client.Client, recipient string) {
	response := bytes.Buffer{}

	for challenge, flag := range flags {
		response.Write([]byte(fmt.Sprintf("%s %d\n", challenge, flag.Points)))
	}

	err := c.SendMessage(recipient, response.String())
	if err != nil {
		log.Println("failed to respond to user: ", err)
	}
}

func sendChallenge(c *client.Client, recipient string, challenge string) {
	flag, ok := flags[challenge]
	if !ok {
		err := c.SendMessage(recipient, fmt.Sprintf("challenge %s does not exist", challenge))
		if err != nil {
			log.Println("failed to respond to user: ", err)
		}
	}

	err := c.SendMessage(recipient, fmt.Sprintf("%s - %d\n\n%s", challenge, flag.Points, flag.Hint))
	if err != nil {
		log.Println("failed to respond to user: ", err)
	}
}
