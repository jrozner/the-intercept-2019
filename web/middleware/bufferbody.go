package middleware

import (
	"bytes"
	"io"
	"io/ioutil"
	"log"
	"net/http"
)

type ReadSeekCloser interface {
	io.Reader
	io.Seeker
	io.Closer
}

type nopCloseSeeker struct {
	io.ReadSeeker
}

func (n nopCloseSeeker) Close() error {
	return nil
}

func BufferBody(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body, err := ioutil.ReadAll(r.Body)
		if err != nil {
			log.Panic(err)
		}

		err = r.Body.Close()
		if err != nil {
			log.Panic(err)
		}

		r.Body = nopCloseSeeker{bytes.NewReader(body)}
		next.ServeHTTP(w, r)
	})
}
