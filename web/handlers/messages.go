package handlers

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"strconv"

	"github.com/go-chi/chi"
	"github.com/jinzhu/gorm"

	"github.com/jrozner/the-intercept-2019/web/model"
)

func getAllMessages(w http.ResponseWriter, r *http.Request) {
	db, ok := r.Context().Value("db").(*gorm.DB)
	if !ok {
		log.Panic("unable to retrieve database")
	}

	user, ok := r.Context().Value("user").(*model.User)
	if !ok {
		log.Panic("unable to retrieve user")
	}

	err := db.Model(&user).Preload("Sender").Related(&user.Messages, "SentTo").Error
	if err != nil && err != gorm.ErrRecordNotFound {
		log.Panic(err)
	}

	w.Header().Add("Content-Type", "application/json")
	err = json.NewEncoder(w).Encode(user.Messages)
	if err != nil {
		log.Panic(err)
	}
}

func getUnreadMessages(w http.ResponseWriter, r *http.Request) {
	db, ok := r.Context().Value("db").(*gorm.DB)
	if !ok {
		log.Panic("unable to retrieve database")
	}

	user, ok := r.Context().Value("user").(*model.User)
	if !ok {
		log.Panic("unable to retrieve user")
	}

	err := db.Model(&user).Preload("Sender").Where("messages.seen = false").Related(&user.Messages, "SentTo").Error
	if err != nil && err != gorm.ErrRecordNotFound {
		log.Panic(err)
	}

	w.Header().Add("Content-Type", "application/json")
	err = json.NewEncoder(w).Encode(user.Messages)
	if err != nil {
		log.Panic(err)
	}
}

func getMessage(w http.ResponseWriter, r *http.Request) {
	db, ok := r.Context().Value("db").(*gorm.DB)
	if !ok {
		log.Panic("unable to retrieve database")
	}

	user, ok := r.Context().Value("user").(*model.User)
	if !ok {
		log.Panic("unable to retrieve user")
	}

	idString := chi.URLParam(r, "id")
	id, err := strconv.ParseUint(idString, 10, 64)
	if err != nil {
		log.Panic(err)
	}

	var message model.Message
	err = db.Model(&user).Preload("Sender").Where("id = ?", id).Related(&message, "SentTo").Error
	if err != nil {
		if err == gorm.ErrRecordNotFound {
			w.WriteHeader(http.StatusNotFound)
			return
		}

		log.Panic(err)
	}

	message.Seen = true

	err = db.Save(&message).Error
	if err != nil {
		log.Println(err)
	}

	w.Header().Add("Content-Type", "application/json")
	err = json.NewEncoder(w).Encode(message)
	if err != nil {
		log.Panic(err)
	}
}

func createMessage(w http.ResponseWriter, r *http.Request) {
	db, ok := r.Context().Value("db").(*gorm.DB)
	if !ok {
		log.Panic("unable to retrieve database")
	}

	user, ok := r.Context().Value("user").(*model.User)
	if !ok {
		log.Panic("unable to retrieve user")
	}

	var receiver model.User
	err := db.First(&receiver, "team_name = ?", r.FormValue("receiver")).Error
	if err != nil {
		w.WriteHeader(http.StatusBadRequest)
		return
	}

	message := model.Message{
		Text:   r.FormValue("text"),
		SentTo: receiver.ID,
		Seen:   false,
	}

	user.SentMessages = append(user.SentMessages, message)

	err = db.Save(&user).Error
	if err != nil {
		log.Panic(err)
	}

	w.Header().Add("Location", fmt.Sprintf("/messages/%d", user.SentMessages[0].ID))
	w.WriteHeader(http.StatusCreated)
}

func deleteMessage(w http.ResponseWriter, r *http.Request) {
	db, ok := r.Context().Value("db").(*gorm.DB)
	if !ok {
		log.Panic("unable to retrieve database")
	}

	user, ok := r.Context().Value("user").(*model.User)
	if !ok {
		log.Panic("unable to retrieve user")
	}

	idString := chi.URLParam(r, "id")
	id, err := strconv.ParseUint(idString, 10, 64)
	if err != nil {
		log.Panic(err)
	}

	var message model.Message
	err = db.Model(&user).Preload("Sender").Where("id = ?", id).Related(&message, "SentTo").Error
	if err != nil {
		if err == gorm.ErrRecordNotFound {
			w.WriteHeader(http.StatusNotFound)
			return
		}

		log.Panic(err)
	}

	err = db.Delete(&message).Error
	if err != nil {
		if err == gorm.ErrRecordNotFound {
			w.WriteHeader(http.StatusNotFound)
			return
		}

		log.Panic(err)
	}

	w.WriteHeader(http.StatusOK)
}
