namespace WebRole1.Controllers
{
    using System;
    using System.Collections.Generic;
    using System.Linq;
    using System.Net;
    using System.Net.Http;
    using System.Web.Http;
    using System.Threading.Tasks;
    using WebRole1.DTO;

    public class RegistrarController : ApiController
    {
        public const string Prefix = "registrar";
        public const string DefaultAction = "registrations";
        public const string RegisterAction = "registerAction";
        public const string UnregisterAction = "unregisterAction";
                
        [HttpGet]
        [ActionName(DefaultAction)]
        public HttpResponseMessage GetRegistrations(string userName)
        {
            Registrations registrations;
            lock (syncRoot)
            {
                if (!allRegistrations.TryGetValue(userName, out registrations))
                    return Request.CreateResponse(HttpStatusCode.NotFound, new UserRegistration[] { });
                return Request.CreateResponse(HttpStatusCode.OK, 
                    registrations.Nodes.Select(n => new UserRegistration() { NodeId = n.Key, Url = n.Value.url }).ToArray());
            }
        }


        [HttpPost]
        [ActionName(RegisterAction)]
        public async Task<HttpResponseMessage> Register(string userName, HttpRequestMessage req)
        {
            var newRegistration = await req.Content.ReadAsAsync<UserRegistration>();
            Registrations registrations;
            lock (syncRoot)
            {
                if (!allRegistrations.TryGetValue(userName, out registrations))
                    allRegistrations.Add(userName, registrations = new Registrations());
                Node node;
                if (!registrations.Nodes.TryGetValue(newRegistration.NodeId, out node))
                    registrations.Nodes.Add(newRegistration.NodeId, node = new Node());
                node.url = newRegistration.Url;
            }
            return Request.CreateResponse(HttpStatusCode.OK);
        }

        [HttpDelete]
        [ActionName(UnregisterAction)]
        public HttpResponseMessage Unregister(string userName, string nodeId)
        {
            Registrations registrations;
            lock (syncRoot)
            {
                if (allRegistrations.TryGetValue(userName, out registrations))
                {
                    registrations.Nodes.Remove(nodeId);
                    if (registrations.Nodes.Count == 0)
                        allRegistrations.Remove(userName);
                }                   
            }
            return Request.CreateResponse(HttpStatusCode.OK);
        }

        class Node
        {
            public string url;
        };

        class Registrations
        {
            public Dictionary<string, Node> Nodes = new Dictionary<string, Node>();
        };

        readonly object syncRoot = new object();
        private static Dictionary<string, Registrations> allRegistrations = new Dictionary<string, Registrations>();
    }
}
