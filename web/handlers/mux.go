package handlers

import "github.com/go-chi/chi"

var Mux = chi.NewMux()

func init() {
	Mux.Get("/contacts", getContacts)
	Mux.Route("/messages", func(r chi.Router) {
		r.Get("/", getMessages)
		r.Get("/{id}", getMessage)
		r.Post("/", createMessage)
		r.Delete("/{id}", deleteMessage)
	})
}
