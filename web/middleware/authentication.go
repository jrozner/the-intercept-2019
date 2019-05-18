package middleware

import (
	"context"
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"hash"
	"io"
	"log"
	"net/http"
	"strings"

	"github.com/jrozner/the-intercept-2019/web/model"

	"github.com/jinzhu/gorm"
)

type authenticate struct {
	db *gorm.DB
	h  http.Handler
}

func (a *authenticate) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	//Authorization: AccessKey=...;Signature=asdfasd
	var (
		accessKey                string
		authorizationParts       = make(map[string]string)
		b                        io.Seeker
		computedSignature        []byte
		ctx                      context.Context
		err                      error
		expectedSignature        string
		expectedSignatureDecoded []byte
		headerParts              []string
		mac                      hash.Hash
		ok                       bool
		secret                   []byte
		status                   = http.StatusInternalServerError
		user                     model.User
	)
	authorizationHeader := r.Header.Get("Authorization")
	if authorizationHeader == "" {
		log.Println("authentication: missing authorization header")
		goto authfail
	}

	headerParts = strings.Split(authorizationHeader, ";")

	for _, part := range headerParts {
		kv := strings.Split(part, "=")
		if len(kv) != 2 {
			log.Println("authentication: malformed authorization header")
			continue
		}

		authorizationParts[kv[0]] = kv[1]
	}

	accessKey, ok = authorizationParts["AccessKey"]
	if !ok {
		log.Println("authentication: missing access key")
		goto authfail
	}

	err = a.db.Model(&user).First(&user, "access_key = ?", accessKey).Error
	if err != nil {
		if err == gorm.ErrRecordNotFound {
			log.Println("authentication: user not found for access key")
			goto authfail
		}

		goto autherr
	}

	expectedSignature, ok = authorizationParts["Signature"]
	if !ok {
		log.Println("authentication: missing signature")
		goto authfail
	}

	secret, err = hex.DecodeString(user.SecretKey)
	if err != nil {
		goto autherr
	}

	mac = hmac.New(sha256.New, secret)

	mac.Write([]byte(r.Method))
	mac.Write([]byte(user.Serial))

	_, err = io.Copy(mac, r.Body)
	if err != nil {
		goto autherr
	}

	computedSignature = mac.Sum(nil)

	b, ok = r.Body.(io.Seeker)
	if ok {
		_, err = b.Seek(0, 0)
		if err != nil {
			log.Println("authentication: unable to seek body back to beginning")
		}
	} else {
		log.Println("authentication: body is not a Seeker")
	}

	expectedSignatureDecoded, err = hex.DecodeString(expectedSignature)
	if err != nil {
		goto autherr
	}

	if !hmac.Equal(expectedSignatureDecoded, computedSignature) {
		log.Println("authentication: computed signature did not match")
		goto authfail
	}

	ctx = context.WithValue(r.Context(), "user", &user)
	r = r.WithContext(ctx)

	a.h.ServeHTTP(w, r)
	return

authfail:
	status = http.StatusUnauthorized

autherr:
	if err != nil {
		log.Println(err)
	}

	http.Error(w, http.StatusText(status), status)
}

func Authenticate(db *gorm.DB) func(next http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return &authenticate{
			db: db,
			h:  next,
		}
	}
}
