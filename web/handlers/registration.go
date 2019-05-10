package handlers

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"log"
	"net/http"

	"github.com/jinzhu/gorm"

	"github.com/jrozner/the-intercept-2019/web/model"
)

type keys struct {
	AccessKey string `json:"access_key"`
	SecretKey string `json:"secret_key"`
}

func Register(w http.ResponseWriter, r *http.Request) {
	db, ok := r.Context().Value("db").(*gorm.DB)
	if !ok {
		log.Panic("unable to retrieve database")
	}

	accessKey := make([]byte, 64)
	_, err := rand.Read(accessKey)
	if err != nil {
		log.Panic(err)
	}

	secretKey := make([]byte, 64)
	_, err = rand.Read(secretKey)
	if err != nil {
		log.Panic(err)
	}

	firstName := r.FormValue("first_name")
	if firstName == "" {
		w.WriteHeader(http.StatusBadRequest)
		return
	}

	lastName := r.FormValue("last_name")
	if lastName == "" {
		w.WriteHeader(http.StatusBadRequest)
		return
	}

	serial := r.FormValue("serial")
	if serial == "" {
		w.WriteHeader(http.StatusBadRequest)
		return
	}

	user := model.User{
		FirstName: firstName,
		LastName:  lastName,
		Serial:    serial,
		AccessKey: hex.EncodeToString(accessKey),
		SecretKey: hex.EncodeToString(secretKey),
	}

	err = db.Save(&user).Error
	if err != nil {
		// TODO: should really handle the case here of an already registered serial number
		log.Panic(err)
	}

	keys := keys{
		AccessKey: user.AccessKey,
		SecretKey: user.SecretKey,
	}

	err = json.NewEncoder(w).Encode(keys)
	if err != nil {
		log.Panic(err)
	}
}
