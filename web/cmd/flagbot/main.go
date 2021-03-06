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

	c, err := client.NewClient(config.Host, config.AccessKey, config.SecretKey[0:64], config.Serial)
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
				sendChallenges(db, c, recipient)
			case "challenge":
				sendChallenge(db, c, recipient, remainder)
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

func sendChallenges(db *gorm.DB, c *client.Client, recipient string) {
	response := bytes.Buffer{}

	var flags []model.Flag
	err := db.Find(&flags).Error
	if err != nil {
		log.Println(err)
		return
	}

	for _, flag := range flags {
		response.Write([]byte(fmt.Sprintf("%s - %d\n", flag.Name, flag.Points)))
	}

	err = c.SendMessage(recipient, response.String())
	if err != nil {
		log.Println("failed to respond to user: ", err)
	}
}

func sendChallenge(db *gorm.DB, c *client.Client, recipient string, challenge string) {
	var flag model.Flag
	err := db.First(&flag, "name = ?", challenge).Error
	if err != nil {
		if err == gorm.ErrRecordNotFound {
			err := c.SendMessage(recipient, fmt.Sprintf("challenge %s does not exist", challenge))
			if err != nil {
				log.Println("failed to respond to user: ", err)
			}
		}

		log.Println(err)
		return
	}

	err = c.SendMessage(recipient, fmt.Sprintf("%s - %d\n\n%s", flag.Name, flag.Points, flag.Hint))
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

	var flag model.Flag
	err := db.First(&flag, "name = ?", parts[0]).Error
	if err != nil {
		if err == gorm.ErrRecordNotFound {
			err := c.SendMessage(recipient, fmt.Sprintf("challenge %s does not exist", parts[0]))
			if err != nil {
				log.Println("failed to respond to user: ", err)
			}
		}

		log.Println(err)
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
	err = db.First(&user, "team_name = ?", recipient).Association("Flags").Append(&flag).Error
	if err != nil {
		if err == gorm.ErrRecordNotFound {
			log.Printf("sender %s doesn't exist\n", recipient)
		} else {
			log.Println(err)
		}

		return
	}

	err = c.SendMessage(recipient, "flag accepted")
	if err != nil {
		log.Println("failed to respond to user: ", err)
	}
}
