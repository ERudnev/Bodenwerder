# Runtime Release Through Host

## Намерение

Текущий временный фикс для `resource::texture::Runtime` — хранить `device: #system::Device` прямо в runtime quantum и использовать его при release.

Это убирает плохой linear search через `Runtime_group` при destruction и оставляет код честным, пока параллельная система `resource::*` ещё растёт.

## Почему это временно

Архитектурно ownership runtime уже выражен со стороны host:

- `group<texture::Runtime> of resource::DeviceRuntimes`
- `DeviceRuntimes` — настоящий host context для per-device runtime objects

Значит, destruction в идеале не должен восстанавливать ownership из сохранённых данных и не должен требовать от `texture::Runtime` знать `resource::DeviceRuntimes` как тип.

## Гипотеза

Group-driven путь destruction должен передавать host identity в quasi-destructor:

- не обычный `release(Writing, Id, const Quantum&)`
- а release с доступным host-side id
- для `texture::Runtime` этот host id численно совпадает с `system::Device::Id`

Тогда:

- `texture::Runtime` не будет искать в `Runtime_group`
- `texture::Runtime` не нужно будет хранить `device` в своём quantum
- destruction получит достаточно контекста, чтобы напрямую восстановить правильный GL device

## Желаемое конечное состояние

Для host-bound runtime objects логика release должна опираться на host identity, которую даёт framework, а не на:

1. linear reverse lookup через group membership
2. дублированный owner id, сохранённый в runtime quantum

Текущее явное поле `device` допустимо только как промежуточная мера, пока валидируется новая архитектура `resource::*`.
