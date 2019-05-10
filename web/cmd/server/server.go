package main

import (
	"log"
	"net/http"

	"github.com/go-chi/chi"
	"github.com/go-chi/chi/middleware"
	"github.com/jinzhu/gorm"
	_ "github.com/jinzhu/gorm/dialects/mysql"

	"github.com/jrozner/the-intercept-2019/web/handlers"
	mw "github.com/jrozner/the-intercept-2019/web/middleware"
)

func main() {
	db, err := gorm.Open("mysql", "root@unix(/tmp/mysql.sock)/the_intercept?charset=utf8&parseTime=true")
	if err != nil {
		log.Fatal(err)
	}

	router := chi.NewMux()
	router.Use(middleware.Logger)
	router.Use(middleware.RequestID)
	router.Use(middleware.Recoverer)
	router.Use(middleware.WithValue("db", db))

	router.Post("/register", handlers.Register)

	authenticated := chi.NewMux()
	authenticated.Use(mw.Authenticate(db))
	handlers.RegisterAuthenticated(authenticated)
	router.Mount("/", authenticated)

	log.Fatal(http.ListenAndServe(":8080", router))
}
