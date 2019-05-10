package model

import "time"

type User struct {
	ID           uint64     `json:"-"`
	FirstName    string     `json:"first_name"`
	LastName     string     `json:"last_name"`
	Messages     []Message  `json:"-" gorm:"foreignKey:SentTo"`
	SentMessages []Message  `json:"-" gorm:"foreignKey:SentBy"`
	AccessKey    string     `json:"-"`
	SecretKey    string     `json:"-"`
	Serial       string     `json:"serial"`
	CreatedAt    time.Time  `json:"created_at"`
	UpdatedAt    time.Time  `json:"updated_at"`
	DeletedAt    *time.Time `json:"-"`
}
