package handlers

import "github.com/go-chi/chi"

func RegisterAuthenticated(mux *chi.Mux) {
	mux.Get("/contacts", getContacts)
	mux.Post("/rotate_secret", rotateSecret)
	mux.Route("/messages", func(r chi.Router) {
		r.Get("/all", getAllMessages)
		r.Get("/", getUnreadMessages)
		r.Get("/{id}", getMessage)
		r.Post("/", createMessage)
		r.Delete("/{id}", deleteMessage)
	})
}
