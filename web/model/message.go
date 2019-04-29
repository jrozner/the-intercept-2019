package model

import "time"

type Message struct {
	ID        uint64     `json:"id"`
	Text      string     `json:"text"`
	SentBy    uint64     `json:"-"`
	Sender    User       `json:"sender" gorm:"foreignKey:SentBy;association_foreignKey:id"`
	SentTo    uint64     `json:"-"`
	Seen      bool       `json:"seen"`
	CreatedAt time.Time  `json:"created_at"`
	UpdatedAt time.Time  `json:"updated_at"`
	DeletedAt *time.Time `json:"-"`
}
