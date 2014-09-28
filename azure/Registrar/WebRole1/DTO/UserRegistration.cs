namespace WebRole1.DTO
{
    using System.Runtime.Serialization;
    using System.ComponentModel.DataAnnotations;

    [DataContract]
    public class UserRegistration
    {
        [Required, DataMember(Name = "nodeId", IsRequired = true)]
        public string NodeId { get; set; }

        [Url, Required, DataMember(Name = "url", IsRequired = true)]
        public string Url { get; set; }
    }
}
