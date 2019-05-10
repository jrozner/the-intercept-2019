package handlers

import "github.com/go-chi/chi"

func RegisterAuthenticated(mux *chi.Mux) {
	mux.Get("/contacts", getContacts)
	mux.Route("/messages", func(r chi.Router) {
		r.Get("/", getMessages)
		r.Get("/{id}", getMessage)
		r.Post("/", createMessage)
		r.Delete("/{id}", deleteMessage)
	})
}
