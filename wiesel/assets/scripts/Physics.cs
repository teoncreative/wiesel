namespace WieselEngine
{
    public struct RaycastHit
    {
        public Entity Entity;
        public Vector3f Point;
        public Vector3f Normal;
        public float Distance;
    }

    public static class Physics
    {
        public static bool Raycast(Entity source, Vector3f origin, Vector3f direction, float maxDistance, out RaycastHit hit, Entity ignoreEntity = null)
        {
            ulong scenePtr = source.ScenePtr;
            ulong ignoreId = ignoreEntity != null ? ignoreEntity.Id : 0;
            return RaycastInternal(scenePtr, origin, direction, maxDistance, out hit, ignoreId);
        }

        public static bool Raycast(Scene scene, Vector3f origin, Vector3f direction, float maxDistance, out RaycastHit hit)
        {
            return RaycastInternal(scene.Ptr, origin, direction, maxDistance, out hit, 0);
        }

        public static Entity[] OverlapBox(Entity source, Vector3f center, Vector3f halfExtents)
        {
            return OverlapBoxInternal(source.ScenePtr, center, halfExtents);
        }

        public static Entity[] OverlapBox(Scene scene, Vector3f center, Vector3f halfExtents)
        {
            return OverlapBoxInternal(scene.Ptr, center, halfExtents);
        }

        public static Entity[] OverlapSphere(Entity source, Vector3f center, float radius)
        {
            return OverlapSphereInternal(source.ScenePtr, center, radius);
        }

        public static Entity[] OverlapSphere(Scene scene, Vector3f center, float radius)
        {
            return OverlapSphereInternal(scene.Ptr, center, radius);
        }

        private static bool RaycastInternal(ulong scenePtr, Vector3f origin, Vector3f direction, float maxDistance, out RaycastHit hit, ulong ignoreEntity)
        {
            hit = new RaycastHit();
            ulong hitEntity = 0;
            float hitPx = 0, hitPy = 0, hitPz = 0;
            float hitNx = 0, hitNy = 0, hitNz = 0;
            float hitDist = 0;
            bool result = Internals.Physics_Raycast(scenePtr,
                origin.X, origin.Y, origin.Z,
                direction.X, direction.Y, direction.Z, maxDistance,
                ignoreEntity,
                out hitEntity, out hitPx, out hitPy, out hitPz,
                out hitNx, out hitNy, out hitNz, out hitDist);
            if (result)
            {
                hit.Entity = new Entity(scenePtr, hitEntity);
                hit.Point = new Vector3f(hitPx, hitPy, hitPz);
                hit.Normal = new Vector3f(hitNx, hitNy, hitNz);
                hit.Distance = hitDist;
            }
            return result;
        }

        private static Entity[] OverlapBoxInternal(ulong scenePtr, Vector3f center, Vector3f halfExtents)
        {
            ulong[] ids = Internals.Physics_OverlapBox(scenePtr,
                center.X, center.Y, center.Z,
                halfExtents.X, halfExtents.Y, halfExtents.Z);
            Entity[] entities = new Entity[ids.Length];
            for (int i = 0; i < ids.Length; i++)
            {
                entities[i] = new Entity(scenePtr, ids[i]);
            }
            return entities;
        }

        private static Entity[] OverlapSphereInternal(ulong scenePtr, Vector3f center, float radius)
        {
            ulong[] ids = Internals.Physics_OverlapSphere(scenePtr,
                center.X, center.Y, center.Z, radius);
            Entity[] entities = new Entity[ids.Length];
            for (int i = 0; i < ids.Length; i++)
            {
                entities[i] = new Entity(scenePtr, ids[i]);
            }
            return entities;
        }
    }
}