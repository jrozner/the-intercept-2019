package handlers

import (
	"crypto/rand"
	"crypto/sha256"
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

	teamName := r.FormValue("team_name")
	if teamName == "" {
		w.WriteHeader(http.StatusBadRequest)
		return
	}

	serial := r.FormValue("serial")
	if serial == "" {
		w.WriteHeader(http.StatusBadRequest)
		return
	}

	accessKey, err := generateKey(sha256.BlockSize)
	if err != nil {
		log.Panic(err)
	}

	secretKey, err := generateKey(sha256.BlockSize)
	if err != nil {
		log.Panic(err)
	}

	user := model.User{
		TeamName:  teamName,
		Serial:    serial,
		AccessKey: accessKey,
		SecretKey: secretKey,
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

	w.Header().Add("Content-Type", "application/json")
	err = json.NewEncoder(w).Encode(keys)
	if err != nil {
		log.Panic(err)
	}
}

func rotateSecret(w http.ResponseWriter, r *http.Request) {
	db, ok := r.Context().Value("db").(*gorm.DB)
	if !ok {
		log.Panic("unable to retrieve database")
	}

	user, ok := r.Context().Value("user").(*model.User)
	if !ok {
		log.Panic("unable to retrieve user")
	}

	secret, err := generateKey(sha256.BlockSize)
	if err != nil {
		log.Panic(err)
	}

	user.SecretKey = secret

	// NOTE(joe): gorm is shit with transactions so if any of this fails
	// along the way bad shit will happen

	err = db.Save(&user).Error
	if err != nil {
		log.Panic(err)
	}

	keys := &keys{
		AccessKey: user.AccessKey,
		SecretKey: user.SecretKey,
	}

	err = db.Model(&user).Related(&user.Flags).Error
	if err != nil {
		if err != gorm.ErrRecordNotFound {
			log.Panic(err)
		}
	}

	err = db.Delete(user.Flags).Error
	if err != nil {
		log.Panic(err)
	}

	w.Header().Add("Content-Type", "application/json")
	err = json.NewEncoder(w).Encode(keys)
	if err != nil {
		log.Panic(err)
	}
}

func generateKey(size uint) (string, error) {
	key := make([]byte, size)
	_, err := rand.Read(key)
	if err != nil {
		return "", err
	}

	return hex.EncodeToString(key), nil
}
