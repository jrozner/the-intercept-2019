package client

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"strconv"
	"strings"

	"github.com/jrozner/the-intercept-2019/web/model"
)

var ErrInvalidKey = errors.New("invalid secret key")

type Keys struct {
	AccessKey string `json:"access_key"`
	SecretKey string `json:"secret_key"`
}

type Client struct {
	client    http.Client
	host      string
	accessKey string
	secretKey []byte
}

func (c *Client) Get(endpoint string) (*http.Response, error) {
	request, err := http.NewRequest(http.MethodGet, endpoint, nil)
	if err != nil {
		return nil, err
	}

	return c.Do(request)
}

func (c *Client) Post(endpoint string, body url.Values) (*http.Response, error) {
	request, err := http.NewRequest(http.MethodPost, endpoint, strings.NewReader(body.Encode()))
	if err != nil {
		return nil, err
	}

	request.Header.Add("Content-Type", "application/x-www-form-urlencoded")

	return c.Do(request)
}

func (c *Client) Delete(endpoint string) (*http.Response, error) {
	request, err := http.NewRequest(http.MethodDelete, endpoint, nil)
	if err != nil {
		return nil, err
	}

	return c.Do(request)
}

func (c *Client) Do(request *http.Request) (*http.Response, error) {
	signature, err := c.signRequest(request)
	if err != nil {
		return nil, err
	}

	authorizationHeader := fmt.Sprintf("AccessKey=%s;Signature=%s", c.accessKey, signature)
	request.Header.Add("Authorization", authorizationHeader)
	return c.client.Do(request)
}

func (c *Client) GetContacts() ([]model.User, error) {
	response, err := c.Get("/contacts")
	if err != nil {
		return nil, err
	}

	defer func(response *http.Response) {
		err := response.Body.Close()
		if err != nil {
			log.Println("unable to close response body")
		}
	}(response)

	var contacts []model.User
	err = json.NewDecoder(response.Body).Decode(contacts)

	return contacts, nil
}

func (c *Client) SendMessage(receiver, message string) (uint64, error) {
	body := url.Values{}
	body.Add("receiver", receiver)
	body.Add("message", message)

	response, err := c.Post("/messages", body)
	if err != nil {
		return 0, err
	}

	location := response.Header.Get("Location")
	parts := strings.Split(location, "/")
	id, err := strconv.ParseUint(parts[len(parts)-1], 10, 64)
	if err != nil {
		return 0, err
	}

	return id, nil
}

func (c *Client) GetMessages() ([]model.Message, error) {
	response, err := c.Get("/messages")
	if err != nil {
		return nil, err
	}

	defer func(response *http.Response) {
		err := response.Body.Close()
		if err != nil {
			log.Println("unable to close response body")
		}
	}(response)

	var messages []model.Message
	err = json.NewDecoder(response.Body).Decode(messages)

	return messages, nil
}

func (c *Client) GetMessage(id uint64) (*model.Message, error) {
	endpoint := fmt.Sprintf("/messages/%d", id)
	response, err := c.Delete(endpoint)
	if err != nil {
		return nil, err
	}

	defer func(response *http.Response) {
		err := response.Body.Close()
		if err != nil {
			log.Println("unable to close body")
		}
	}(response)

	message := &model.Message{}
	err = json.NewDecoder(response.Body).Decode(&message)
	if err != nil {
		return nil, err
	}

	return message, nil
}

func (c *Client) DeleteMessage(id uint64) error {
	endpoint := fmt.Sprintf("/messages/%d", id)
	response, err := c.Delete(endpoint)
	if err != nil {
		return err
	}

	defer func(response *http.Response) {
		err := response.Body.Close()
		if err != nil {
			log.Println("unable to close body")
		}
	}(response)

	return nil
}

func (c *Client) RotateSecret() {}

// Register sends a registration request to the server and responds with the
// access and secret keys if all went well. Unlike the other high level API
// methods this one specifically bypasses our implementation of Do to avoid
// trying to sign the request as keys should not be available at this time
func (c *Client) Register(teamName, serial string) (*Keys, error) {
	body := url.Values{}
	body.Add("team_name", teamName)
	body.Add("serial", serial)

	response, err := c.client.PostForm("/register", body)
	if err != nil {
		return nil, err
	}

	keys := &Keys{}
	err = json.NewDecoder(response.Body).Decode(&keys)
	if err != nil {
		return nil, err
	}

	return keys, nil
}

func (c *Client) signRequest(request *http.Request) (string, error) {
	h := hmac.New(sha256.New, c.secretKey)
	h.Write([]byte(request.Method))
	_, err := io.Copy(h, request.Body)
	if err != nil {
		return "", err
	}

	digest := hex.EncodeToString(h.Sum(nil))

	return digest, nil
}

func (c *Client) SetAccessKey(accessKey string) {
	c.accessKey = accessKey
}

func (c *Client) SetSecretKey(secretKey string) error {
	secret, err := hex.DecodeString(secretKey)
	if err != nil {
		return ErrInvalidKey
	}

	c.secretKey = secret

	return nil
}

func NewClient(host, accessKey, secretKey string) (*Client, error) {
	secret, err := hex.DecodeString(secretKey)
	if err != nil {
		return nil, ErrInvalidKey
	}

	return &Client{
		client:    http.Client{},
		host:      host,
		accessKey: accessKey,
		secretKey: secret,
	}, nil
}
