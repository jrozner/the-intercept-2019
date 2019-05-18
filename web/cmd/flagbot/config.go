package main

import (
	"encoding/json"
	"log"
	"os"
)

type Config struct {
	DSN       string `json:"dsn"`
	Debug     bool   `json:"debug"`
	Host      string `json:"host"`
	AccessKey string `json:"access_key"`
	SecretKey string `json:"secret_key"`
	Serial    string `json:"serial"`
}

func readConfig() (*Config, error) {
	fp, err := os.Open("config.json")
	if err != nil {
		return nil, err
	}
	defer func(*os.File) {
		err := fp.Close()
		if err != nil {
			log.Println("unable to close config file")
		}
	}(fp)

	config := &Config{}

	err = json.NewDecoder(fp).Decode(config)
	if err != nil {
		return nil, err
	}

	return config, nil
}
