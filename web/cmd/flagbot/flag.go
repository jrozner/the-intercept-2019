package main

import (
	"encoding/json"
	"log"
	"os"
)

type Flag struct {
	Hint   string `json:"hint"`
	Value  string `json:"Value"`
	Points int    `json:"points"`
}

func readFlags() (map[string]Flag, error) {
	fp, err := os.Open("flags.json")
	if err != nil {
		return nil, err
	}
	defer func(*os.File) {
		err := fp.Close()
		if err != nil {
			log.Println("unable to close flags file")
		}
	}(fp)

	flags := make(map[string]Flag)

	err = json.NewDecoder(fp).Decode(&flags)
	if err != nil {
		return nil, err
	}

	return flags, nil
}
