package main

import (
	"bytes"
	"fmt"
	"log"
	"strings"
	"time"

	"github.com/jinzhu/gorm"
	_ "github.com/jinzhu/gorm/dialects/mysql"

	"github.com/jrozner/the-intercept-2019/web/client"
	"github.com/jrozner/the-intercept-2019/web/model"
)

var flags map[string]Flag

func main() {
	config, err := readConfig()
	if err != nil {
		log.Fatal(err)
	}

	db, err := gorm.Open("mysql", config.DSN)
	if err != nil {
		log.Fatal(err)
	}

	if config.Debug {
		db.LogMode(true)
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
				remainder = message.Text[commandIndex+1:]
			}

			switch command {
			case "help":
				sendHelp(c, recipient)
			case "challenges":
				sendChallenges(c, recipient)
			case "challenge":
				sendChallenge(c, recipient, remainder)
			case "solve":
				solveChallenge(db, c, recipient, remainder)
			case "scoreboard":
			default:
				log.Println("invalid command: ", command)
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

func solveChallenge(db *gorm.DB, c *client.Client, recipient string, message string) {
	parts := strings.Split(message, ":")
	if len(parts) != 2 {
		log.Println("malformed solution")
		err := c.SendMessage(recipient, "malformed solution")
		if err != nil {
			log.Println("failed to respond to user: ", err)
		}

		return
	}

	flag, ok := flags[parts[0]]
	if !ok {
		log.Printf("Challenge '%s' doesn't exist\n", parts[0])
		err := c.SendMessage(recipient, fmt.Sprintf("invalid challenge: %s", parts[0]))
		if err != nil {
			log.Println("failed to respond to user: ", err)
		}

		return
	}

	// hey, there's a timing attack possible here. Good fucking luck
	if parts[1] != flag.Value {
		log.Println("invalid flag")
		err := c.SendMessage(recipient, fmt.Sprintf("invalid flag"))
		if err != nil {
			log.Println("failed to respond to user: ", err)
		}

		return
	}

	var user model.User
	err := db.Model(&user).First(&user, "team_name = ?", recipient).Error
	if err != nil {
		if err == gorm.ErrRecordNotFound {
			log.Printf("sender %s doesn't exist\n", recipient)
			return
		}

		log.Println(err)
		return
	}

	newFlag := &model.Flag{
		Challenge: parts[0],
		Points:    flag.Points,
		UserID:    user.ID,
	}

	err = db.Create(&newFlag).Error
	if err != nil {
		log.Println(err)
	}

	err = c.SendMessage(recipient, "flag accepted")
	if err != nil {
		log.Println("failed to respond to user: ", err)
	}
}
