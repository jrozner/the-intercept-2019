package main

import (
	"log"
	"net/http"

	"github.com/go-chi/chi"
	"github.com/go-chi/chi/middleware"
	"github.com/jinzhu/gorm"
	_ "github.com/jinzhu/gorm/dialects/mysql"
	"github.com/jrozner/the-intercept-2019/web/handlers"
)

func main() {
	db, err := gorm.Open("mysql", "root@unix(/tmp/mysql.sock)/the_intercept?charset=utf8&parseTime=True")
	if err != nil {
		log.Fatal(err)
	}
	db.LogMode(true)

	router := chi.NewMux()
	router.Use(middleware.Logger)
	router.Use(middleware.RequestID)
	router.Use(middleware.Recoverer)
	router.Use(middleware.WithValue("db", db))
	router.Use(middleware.WithValue("user", uint64(1)))

	router.Mount("/", handlers.Mux)

	log.Fatal(http.ListenAndServe(":8080", router))
}
