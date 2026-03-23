//
// Created by Metehan Gezer on 15.02.2026.
//

#ifndef WIESEL_PARENT_W_SYSTEM_H
#define WIESEL_PARENT_W_SYSTEM_H

#include <entt/entt.hpp>

namespace Wiesel {

/**
 * @brief Type trait to detect component references
 *
 * Components are identified as reference types (T&).
 * Used to validate system function parameters at compile time.
 */
template <typename T>
struct is_component : std::false_type {};

template <typename T>
struct is_component<T&> : std::true_type {};

/**
 * @brief Alternative system function wrapper using std::function
 *
 * This version uses type erasure to handle generic lambdas from WIESEL_BIND_FN.
 * It's slightly less efficient but more flexible.
 */
template <typename Entity, typename Scene, typename... Components>
class TypedSystemFunction {
 private:
  std::function<void(float, Entity, Components&...)> func;

 public:
  /**
     * @brief Construct from any callable that matches the signature
     */
  template <typename Func>
  explicit TypedSystemFunction(Func f) : func(f) {}

  /**
     * @brief Execute the system function
     */
  void operator()(Scene* scene, float deltatime, entt::entity entity,
                  entt::registry& registry) {
    if constexpr (sizeof...(Components) == 0) {
      func(deltatime, Entity{entity, scene});
    } else {
      std::apply(
          [this, deltatime, entity, scene](auto&&... comps) {
            func(deltatime, Entity{entity, scene}, comps...);
          },
          std::forward_as_tuple(
              registry.get<std::remove_reference_t<Components>>(entity)...));
    }
  }

  /**
     * @brief Check if entity has all required components
     */
  bool HasComponents(entt::entity entity, entt::registry& registry) {
    if constexpr (sizeof...(Components) == 0) {
      return true;
    } else {
      return (registry.any_of<std::remove_reference_t<Components>>(entity) &&
              ...);
    }
  }
};

/**
 * @brief Helper to create TypedSystemFunction with deduced component types
 *
 * Usage: makeSystemFunction<gTransformComponent>(G_BIND_FUNCTION(updateBullets))
 */
template <typename... Components, typename Func>
auto MakeSystemFunction(Func&& f) {
  return TypedSystemFunction<Components...>(std::forward<Func>(f));
}

}  // namespace Wiesel
#endif  //WIESEL_PARENT_W_SYSTEM_H
