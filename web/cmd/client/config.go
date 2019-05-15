package main

import (
	"encoding/json"
	"log"
	"os"
)

type Config struct {
	Host      string `json:"host"`
	AccessKey string `json:"access_key"`
	SecretKey string `json:"secret_key"`
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

func createConfig() (*Config, error) {
	fp, err := os.Create("config.json")
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

	err = json.NewEncoder(fp).Encode(config)
	if err != nil {
		return nil, err
	}

	return config, nil
}

func saveConfig(config *Config) error {
	fp, err := os.OpenFile("config.json", os.O_WRONLY, 0755)
	if err != nil {
		return err
	}
	defer func(*os.File) {
		err := fp.Close()
		if err != nil {
			log.Println("unable to close config file")
		}
	}(fp)

	err = json.NewEncoder(fp).Encode(config)
	if err != nil {
		return err
	}

	return nil
}
