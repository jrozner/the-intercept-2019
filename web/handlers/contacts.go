package handlers

import (
	"encoding/json"
	"log"
	"net/http"

	"github.com/jrozner/the-intercept-2019/web/model"

	"github.com/jinzhu/gorm"
)

func getContacts(w http.ResponseWriter, r *http.Request) {
	db, ok := r.Context().Value("db").(*gorm.DB)
	if !ok {
		log.Panic("unable to retrieve database")
	}

	var users []model.User
	err := db.Find(&users).Error
	if err != nil && err != gorm.ErrRecordNotFound {
		log.Panic(err)
	}

	w.Header().Add("Content-Type", "application/json")
	err = json.NewEncoder(w).Encode(users)
	if err != nil {
		log.Panic(err)
	}
}
